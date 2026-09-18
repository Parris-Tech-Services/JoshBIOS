#include "elf64_plan.h"

static int add_overflows(je_u64 a, je_u64 b) { return ~a < b; }

static int mul_overflows(je_u64 a, je_u64 b) {
    return a != 0 && b > (~(je_u64)0) / a;
}

static je_u64 align_up(je_u64 value, je_u64 alignment, int *overflow) {
    je_u64 mask = alignment - 1u;
    if (add_overflows(value, mask)) {
        *overflow = 1;
        return 0;
    }
    return (value + mask) & ~mask;
}

static int ranges_overlap(je_u64 a, je_u64 a_end, je_u64 b, je_u64 b_end) {
    return a < b_end && b < a_end;
}

JoshElf64PlanStatus josh_elf64_plan_segments(const JoshElf64Summary *summary,
                                              const JoshElf64LoadSegment *segments,
                                              je_u16 segment_count,
                                              je_u64 physical_base,
                                              je_u64 physical_limit,
                                              JoshElf64LoadPlan *plan) {
    if (!summary || !segments || !plan || segment_count == 0 ||
        segment_count != summary->load_segment_count ||
        segment_count > JOSH_ELF64_MAX_PHDRS || physical_base >= physical_limit) {
        return JOSH_ELF64_PLAN_ERR_ARGUMENT;
    }

    plan->entry = summary->entry;
    plan->virtual_min = summary->virtual_min;
    plan->virtual_max = summary->virtual_max;
    plan->mapping_count = 0;

    int overflow = 0;
    je_u64 cursor = align_up(physical_base, JOSH_ELF64_PAGE_SIZE, &overflow);
    if (overflow || cursor >= physical_limit) return JOSH_ELF64_PLAN_ERR_OUT_OF_MEMORY;
    plan->physical_start = cursor;

    for (je_u16 i = 0; i < segment_count; ++i) {
        const JoshElf64LoadSegment *segment = &segments[i];
        if (segment->memory_size == 0 || segment->file_size > segment->memory_size) {
            return JOSH_ELF64_PLAN_ERR_ELF;
        }

        je_u64 virtual_page = segment->virtual_address & ~((je_u64)JOSH_ELF64_PAGE_SIZE - 1u);
        je_u64 page_offset = segment->virtual_address - virtual_page;
        je_u64 span = 0;
        if (add_overflows(page_offset, segment->memory_size)) return JOSH_ELF64_PLAN_ERR_OVERFLOW;
        span = page_offset + segment->memory_size;
        je_u64 rounded_span = align_up(span, JOSH_ELF64_PAGE_SIZE, &overflow);
        if (overflow || rounded_span == 0) return JOSH_ELF64_PLAN_ERR_OVERFLOW;

        je_u64 virtual_end = 0;
        if (add_overflows(virtual_page, rounded_span)) return JOSH_ELF64_PLAN_ERR_OVERFLOW;
        virtual_end = virtual_page + rounded_span;

        for (je_u16 j = 0; j < plan->mapping_count; ++j) {
            const JoshElf64Mapping *other = &plan->mappings[j];
            if (mul_overflows(other->page_count, JOSH_ELF64_PAGE_SIZE)) return JOSH_ELF64_PLAN_ERR_OVERFLOW;
            je_u64 other_span = other->page_count * JOSH_ELF64_PAGE_SIZE;
            if (add_overflows(other->virtual_page, other_span)) return JOSH_ELF64_PLAN_ERR_OVERFLOW;
            je_u64 other_end = other->virtual_page + other_span;
            if (ranges_overlap(virtual_page, virtual_end, other->virtual_page, other_end)) {
                return JOSH_ELF64_PLAN_ERR_PAGE_OVERLAP;
            }
        }

        je_u64 physical_page = align_up(cursor, JOSH_ELF64_PAGE_SIZE, &overflow);
        if (overflow || add_overflows(physical_page, rounded_span)) return JOSH_ELF64_PLAN_ERR_OUT_OF_MEMORY;
        je_u64 physical_end = physical_page + rounded_span;
        if (physical_end > physical_limit) return JOSH_ELF64_PLAN_ERR_OUT_OF_MEMORY;

        JoshElf64Mapping *mapping = &plan->mappings[plan->mapping_count++];
        mapping->virtual_page = virtual_page;
        mapping->physical_page = physical_page;
        mapping->physical_address = physical_page + page_offset;
        mapping->page_count = rounded_span / JOSH_ELF64_PAGE_SIZE;
        mapping->file_offset = segment->file_offset;
        mapping->file_size = segment->file_size;
        mapping->memory_size = segment->memory_size;
        mapping->flags = segment->flags;
        cursor = physical_end;
    }

    plan->physical_end = cursor;
    return JOSH_ELF64_PLAN_OK;
}

JoshElf64PlanStatus josh_elf64_plan_image(const void *image,
                                           je_u64 image_size,
                                           je_u64 physical_base,
                                           je_u64 physical_limit,
                                           JoshElf64LoadPlan *plan) {
    if (!image || !plan) return JOSH_ELF64_PLAN_ERR_ARGUMENT;
    JoshElf64Summary summary;
    JoshElf64Status elf_status = josh_elf64_validate(image, image_size, &summary);
    if (elf_status != JOSH_ELF64_OK) return JOSH_ELF64_PLAN_ERR_ELF;
    if (summary.load_segment_count == 0 || summary.load_segment_count > JOSH_ELF64_MAX_PHDRS) {
        return JOSH_ELF64_PLAN_ERR_ELF;
    }

    JoshElf64LoadSegment segments[JOSH_ELF64_MAX_PHDRS];
    for (je_u16 i = 0; i < summary.load_segment_count; ++i) {
        elf_status = josh_elf64_load_segment(image, image_size, i, &segments[i]);
        if (elf_status != JOSH_ELF64_OK) return JOSH_ELF64_PLAN_ERR_ELF;
    }
    return josh_elf64_plan_segments(&summary, segments, summary.load_segment_count,
                                     physical_base, physical_limit, plan);
}

JoshElf64PlanStatus josh_elf64_materialize(const void *image,
                                            je_u64 image_size,
                                            const JoshElf64LoadPlan *plan,
                                            JoshElf64WriteFn write_fn,
                                            JoshElf64ZeroFn zero_fn,
                                            void *context) {
    if (!image || !plan || !write_fn || !zero_fn ||
        plan->mapping_count == 0 || plan->mapping_count > JOSH_ELF64_MAX_PHDRS) {
        return JOSH_ELF64_PLAN_ERR_ARGUMENT;
    }
    const je_u8 *bytes = (const je_u8 *)image;
    for (je_u16 i = 0; i < plan->mapping_count; ++i) {
        const JoshElf64Mapping *mapping = &plan->mappings[i];
        if (mapping->file_size > mapping->memory_size ||
            add_overflows(mapping->file_offset, mapping->file_size) ||
            mapping->file_offset + mapping->file_size > image_size) {
            return JOSH_ELF64_PLAN_ERR_ELF;
        }
        if (mul_overflows(mapping->page_count, JOSH_ELF64_PAGE_SIZE)) {
            return JOSH_ELF64_PLAN_ERR_OVERFLOW;
        }
        je_u64 mapped_bytes = mapping->page_count * JOSH_ELF64_PAGE_SIZE;
        if (zero_fn(context, mapping->physical_page, mapped_bytes) != 0) {
            return JOSH_ELF64_PLAN_ERR_IO;
        }
        if (mapping->file_size != 0 &&
            write_fn(context, mapping->physical_address,
                     bytes + mapping->file_offset, mapping->file_size) != 0) {
            return JOSH_ELF64_PLAN_ERR_IO;
        }
    }
    return JOSH_ELF64_PLAN_OK;
}

const char *josh_elf64_plan_status_string(JoshElf64PlanStatus status) {
    switch (status) {
        case JOSH_ELF64_PLAN_OK: return "ok";
        case JOSH_ELF64_PLAN_ERR_ARGUMENT: return "invalid argument";
        case JOSH_ELF64_PLAN_ERR_ELF: return "invalid ELF load layout";
        case JOSH_ELF64_PLAN_ERR_OVERFLOW: return "ELF load layout overflow";
        case JOSH_ELF64_PLAN_ERR_PAGE_OVERLAP: return "ELF segments share a page";
        case JOSH_ELF64_PLAN_ERR_OUT_OF_MEMORY: return "insufficient physical memory";
        case JOSH_ELF64_PLAN_ERR_IO: return "ELF materialization I/O failure";
        default: return "unknown ELF load-plan error";
    }
}
