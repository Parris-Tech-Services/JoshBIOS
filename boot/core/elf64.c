#include "elf64.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ELF64_HEADER_SIZE 64u
#define ELF64_PHDR_SIZE 56u
#define ELFCLASS64 2u
#define ELFDATA2LSB 1u
#define EV_CURRENT 1u
#define ET_EXEC 2u
#define EM_X86_64 62u
#define PT_LOAD 1u
#define PF_X 1u

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t le64(const uint8_t *p) {
    return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
}

static int add_overflow_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (UINT64_MAX - a < b) return 1;
    *out = a + b;
    return 0;
}

static int range_overlap(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1) {
    return a0 < b1 && b0 < a1;
}

static int power_of_two_u64(uint64_t v) {
    return v != 0 && (v & (v - 1u)) == 0;
}

static uint64_t align_up(uint64_t value, uint64_t alignment, int *overflow) {
    uint64_t mask = alignment - 1u;
    if (value > UINT64_MAX - mask) {
        *overflow = 1;
        return 0;
    }
    return (value + mask) & ~mask;
}

josh_elf64_status_t josh_elf64_parse(const void *image, size_t image_size,
                                      josh_elf64_image_t *elf) {
    if (!image || !elf || image_size < ELF64_HEADER_SIZE) return JOSH_ELF64_TOO_SMALL;
    const uint8_t *bytes = (const uint8_t *)image;
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F') return JOSH_ELF64_BAD_MAGIC;
    if (bytes[4] != ELFCLASS64) return JOSH_ELF64_UNSUPPORTED_CLASS;
    if (bytes[5] != ELFDATA2LSB) return JOSH_ELF64_UNSUPPORTED_ENDIAN;
    if (bytes[6] != EV_CURRENT || le32(bytes + 20) != EV_CURRENT) return JOSH_ELF64_CORRUPT;
    if (le16(bytes + 16) != ET_EXEC) return JOSH_ELF64_UNSUPPORTED_TYPE;
    if (le16(bytes + 18) != EM_X86_64) return JOSH_ELF64_UNSUPPORTED_MACHINE;
    if (le16(bytes + 52) < ELF64_HEADER_SIZE || le16(bytes + 54) != ELF64_PHDR_SIZE) return JOSH_ELF64_CORRUPT;

    uint64_t phoff = le64(bytes + 32);
    uint16_t phnum = le16(bytes + 56);
    if (phnum == 0) return JOSH_ELF64_CORRUPT;
    uint64_t phbytes = (uint64_t)phnum * ELF64_PHDR_SIZE;
    uint64_t phend = 0;
    if (add_overflow_u64(phoff, phbytes, &phend) || phend > image_size) return JOSH_ELF64_CORRUPT;

    memset(elf, 0, sizeof(*elf));
    elf->entry = le64(bytes + 24);
    elf->virtual_min = UINT64_MAX;

    for (uint16_t i = 0; i < phnum; ++i) {
        const uint8_t *ph = bytes + phoff + (uint64_t)i * ELF64_PHDR_SIZE;
        if (le32(ph) != PT_LOAD) continue;
        if (elf->segment_count >= JOSH_ELF64_MAX_SEGMENTS) return JOSH_ELF64_TOO_MANY_SEGMENTS;

        josh_elf64_segment_t seg;
        seg.flags = le32(ph + 4);
        seg.file_offset = le64(ph + 8);
        seg.virtual_address = le64(ph + 16);
        seg.original_physical_address = le64(ph + 24);
        seg.file_size = le64(ph + 32);
        seg.memory_size = le64(ph + 40);
        seg.alignment = le64(ph + 48);

        if (seg.memory_size == 0) continue;
        if (seg.file_size > seg.memory_size) return JOSH_ELF64_CORRUPT;
        uint64_t file_end = 0;
        if (add_overflow_u64(seg.file_offset, seg.file_size, &file_end) || file_end > image_size) return JOSH_ELF64_CORRUPT;
        if (seg.alignment > 1) {
            if (!power_of_two_u64(seg.alignment)) return JOSH_ELF64_UNSUPPORTED_ALIGNMENT;
            if ((seg.virtual_address & (seg.alignment - 1u)) != (seg.file_offset & (seg.alignment - 1u))) {
                return JOSH_ELF64_UNSUPPORTED_ALIGNMENT;
            }
        }
        uint64_t virtual_end = 0;
        if (add_overflow_u64(seg.virtual_address, seg.memory_size, &virtual_end)) return JOSH_ELF64_CORRUPT;

        for (uint16_t s = 0; s < elf->segment_count; ++s) {
            const josh_elf64_segment_t *other = &elf->segments[s];
            uint64_t other_end = other->virtual_address + other->memory_size;
            if (range_overlap(seg.virtual_address, virtual_end, other->virtual_address, other_end)) return JOSH_ELF64_OVERLAP;
        }

        if (seg.virtual_address < elf->virtual_min) elf->virtual_min = seg.virtual_address;
        if (virtual_end > elf->virtual_max) elf->virtual_max = virtual_end;
        elf->segments[elf->segment_count++] = seg;
    }

    if (elf->segment_count == 0) return JOSH_ELF64_CORRUPT;
    int executable_entry = 0;
    for (uint16_t i = 0; i < elf->segment_count; ++i) {
        const josh_elf64_segment_t *seg = &elf->segments[i];
        uint64_t end = seg->virtual_address + seg->memory_size;
        if ((seg->flags & PF_X) && elf->entry >= seg->virtual_address && elf->entry < end) {
            executable_entry = 1;
            break;
        }
    }
    if (!executable_entry) return JOSH_ELF64_ENTRY_NOT_EXECUTABLE;
    return JOSH_ELF64_OK;
}

josh_elf64_status_t josh_elf64_plan(const josh_elf64_image_t *elf,
                                     uint64_t physical_base,
                                     uint64_t physical_limit,
                                     josh_elf64_load_plan_t *plan) {
    if (!elf || !plan || physical_base >= physical_limit || elf->segment_count == 0) return JOSH_ELF64_OUT_OF_MEMORY;
    memset(plan, 0, sizeof(*plan));
    plan->entry = elf->entry;
    int overflow = 0;
    uint64_t cursor = align_up(physical_base, JOSH_ELF64_PAGE_SIZE, &overflow);
    if (overflow || cursor >= physical_limit) return JOSH_ELF64_OUT_OF_MEMORY;
    plan->physical_start = cursor;

    for (uint16_t i = 0; i < elf->segment_count; ++i) {
        const josh_elf64_segment_t *seg = &elf->segments[i];
        uint64_t vpage = seg->virtual_address & ~(uint64_t)(JOSH_ELF64_PAGE_SIZE - 1u);
        uint64_t page_offset = seg->virtual_address - vpage;
        uint64_t span = 0;
        if (add_overflow_u64(page_offset, seg->memory_size, &span)) return JOSH_ELF64_OUT_OF_MEMORY;
        uint64_t span_aligned = align_up(span, JOSH_ELF64_PAGE_SIZE, &overflow);
        if (overflow || span_aligned == 0) return JOSH_ELF64_OUT_OF_MEMORY;

        uint64_t vend = 0;
        if (add_overflow_u64(vpage, span_aligned, &vend)) return JOSH_ELF64_OUT_OF_MEMORY;
        for (uint16_t j = 0; j < i; ++j) {
            const josh_elf64_load_segment_t *other = &plan->segments[j];
            uint64_t other_span = 0;
            uint64_t other_vend = 0;
            if (other->page_count > UINT64_MAX / JOSH_ELF64_PAGE_SIZE) return JOSH_ELF64_OUT_OF_MEMORY;
            other_span = other->page_count * JOSH_ELF64_PAGE_SIZE;
            if (add_overflow_u64(other->virtual_page, other_span, &other_vend)) return JOSH_ELF64_OUT_OF_MEMORY;
            if (range_overlap(vpage, vend, other->virtual_page, other_vend)) return JOSH_ELF64_OVERLAP;
        }

        uint64_t phys_page = align_up(cursor, JOSH_ELF64_PAGE_SIZE, &overflow);
        uint64_t phys_end = 0;
        if (overflow || add_overflow_u64(phys_page, span_aligned, &phys_end) || phys_end > physical_limit) {
            return JOSH_ELF64_OUT_OF_MEMORY;
        }

        josh_elf64_load_segment_t *dst = &plan->segments[plan->segment_count++];
        dst->virtual_page = vpage;
        dst->physical_page = phys_page;
        dst->physical_address = phys_page + page_offset;
        dst->page_count = span_aligned / JOSH_ELF64_PAGE_SIZE;
        dst->file_offset = seg->file_offset;
        dst->file_size = seg->file_size;
        dst->memory_size = seg->memory_size;
        dst->flags = seg->flags;
        cursor = phys_end;
    }
    plan->physical_end = cursor;
    return JOSH_ELF64_OK;
}

josh_elf64_status_t josh_elf64_load(const void *image, size_t image_size,
                                     const josh_elf64_load_plan_t *plan,
                                     josh_elf64_write_fn write_fn,
                                     josh_elf64_zero_fn zero_fn,
                                     void *context) {
    if (!image || !plan || !write_fn || !zero_fn) return JOSH_ELF64_IO_ERROR;
    const uint8_t *bytes = (const uint8_t *)image;
    for (uint16_t i = 0; i < plan->segment_count; ++i) {
        const josh_elf64_load_segment_t *seg = &plan->segments[i];
        uint64_t end = 0;
        if (add_overflow_u64(seg->file_offset, seg->file_size, &end) || end > image_size) return JOSH_ELF64_CORRUPT;
        if (seg->file_size > SIZE_MAX || seg->memory_size - seg->file_size > SIZE_MAX) return JOSH_ELF64_UNSUPPORTED_ALIGNMENT;
        if (seg->file_size && write_fn(context, seg->physical_address, bytes + seg->file_offset, (size_t)seg->file_size) != 0) {
            return JOSH_ELF64_IO_ERROR;
        }
        uint64_t zero_start = 0;
        if (add_overflow_u64(seg->physical_address, seg->file_size, &zero_start)) return JOSH_ELF64_CORRUPT;
        uint64_t zero_size = seg->memory_size - seg->file_size;
        if (zero_size && zero_fn(context, zero_start, (size_t)zero_size) != 0) return JOSH_ELF64_IO_ERROR;
    }
    return JOSH_ELF64_OK;
}

const char *josh_elf64_status_string(josh_elf64_status_t status) {
    switch (status) {
        case JOSH_ELF64_OK: return "ok";
        case JOSH_ELF64_TOO_SMALL: return "ELF too small";
        case JOSH_ELF64_BAD_MAGIC: return "bad ELF magic";
        case JOSH_ELF64_UNSUPPORTED_CLASS: return "unsupported ELF class";
        case JOSH_ELF64_UNSUPPORTED_ENDIAN: return "unsupported ELF endian";
        case JOSH_ELF64_UNSUPPORTED_TYPE: return "unsupported ELF type";
        case JOSH_ELF64_UNSUPPORTED_MACHINE: return "unsupported ELF machine";
        case JOSH_ELF64_CORRUPT: return "corrupt ELF";
        case JOSH_ELF64_TOO_MANY_SEGMENTS: return "too many ELF load segments";
        case JOSH_ELF64_OVERLAP: return "overlapping ELF segments";
        case JOSH_ELF64_UNSUPPORTED_ALIGNMENT: return "unsupported ELF alignment";
        case JOSH_ELF64_ENTRY_NOT_EXECUTABLE: return "ELF entry is not executable";
        case JOSH_ELF64_OUT_OF_MEMORY: return "ELF load plan out of memory";
        case JOSH_ELF64_IO_ERROR: return "ELF load I/O error";
        default: return "unknown ELF64 error";
    }
}
