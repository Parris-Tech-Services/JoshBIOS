#include "elf64.h"

#define EI_NIDENT 16u
#define ELFCLASS64 2u
#define ELFDATA2LSB 1u
#define EV_CURRENT 1u
#define ET_EXEC 2u
#define EM_X86_64 62u

#pragma pack(push, 1)
typedef struct {
    je_u8 ident[EI_NIDENT];
    je_u16 type;
    je_u16 machine;
    je_u32 version;
    je_u64 entry;
    je_u64 phoff;
    je_u64 shoff;
    je_u32 flags;
    je_u16 ehsize;
    je_u16 phentsize;
    je_u16 phnum;
    je_u16 shentsize;
    je_u16 shnum;
    je_u16 shstrndx;
} Elf64Header;

typedef struct {
    je_u32 type;
    je_u32 flags;
    je_u64 offset;
    je_u64 vaddr;
    je_u64 paddr;
    je_u64 filesz;
    je_u64 memsz;
    je_u64 align;
} Elf64ProgramHeader;
#pragma pack(pop)

static int add_overflows(je_u64 a, je_u64 b) {
    return ~a < b;
}

static int range_inside(je_u64 offset, je_u64 length, je_u64 total) {
    return !add_overflows(offset, length) && offset + length <= total;
}

static int power_of_two(je_u64 value) {
    return value != 0 && (value & (value - 1u)) == 0;
}

static const Elf64Header *header_if_basic_valid(
    const void *image,
    je_u64 image_size,
    JoshElf64Status *status
) {
    if (!image || !status) return 0;
    if (image_size < sizeof(Elf64Header)) {
        *status = JOSH_ELF64_ERR_TRUNCATED;
        return 0;
    }

    const Elf64Header *h = (const Elf64Header *)image;
    if (h->ident[0] != 0x7f || h->ident[1] != 'E' ||
        h->ident[2] != 'L' || h->ident[3] != 'F') {
        *status = JOSH_ELF64_ERR_MAGIC;
        return 0;
    }
    if (h->ident[4] != ELFCLASS64) {
        *status = JOSH_ELF64_ERR_CLASS;
        return 0;
    }
    if (h->ident[5] != ELFDATA2LSB) {
        *status = JOSH_ELF64_ERR_ENDIAN;
        return 0;
    }
    if (h->ident[6] != EV_CURRENT || h->version != EV_CURRENT) {
        *status = JOSH_ELF64_ERR_VERSION;
        return 0;
    }
    if (h->type != ET_EXEC) {
        *status = JOSH_ELF64_ERR_TYPE;
        return 0;
    }
    if (h->machine != EM_X86_64) {
        *status = JOSH_ELF64_ERR_MACHINE;
        return 0;
    }
    if (h->ehsize != sizeof(Elf64Header) ||
        h->phentsize != sizeof(Elf64ProgramHeader) ||
        h->phnum == 0 || h->phnum > JOSH_ELF64_MAX_PHDRS) {
        *status = JOSH_ELF64_ERR_PHDR;
        return 0;
    }
    if (!range_inside(h->phoff, (je_u64)h->phnum * h->phentsize, image_size)) {
        *status = JOSH_ELF64_ERR_TRUNCATED;
        return 0;
    }

    *status = JOSH_ELF64_OK;
    return h;
}

static const Elf64ProgramHeader *program_header(
    const void *image,
    const Elf64Header *h,
    je_u16 index
) {
    const je_u8 *base = (const je_u8 *)image;
    return (const Elf64ProgramHeader *)(base + h->phoff +
                                        (je_u64)index * h->phentsize);
}

static JoshElf64Status validate_load_segment(
    const Elf64ProgramHeader *p,
    je_u64 image_size
) {
    if (p->filesz > p->memsz) return JOSH_ELF64_ERR_SEGMENT;
    if (!range_inside(p->offset, p->filesz, image_size)) {
        return JOSH_ELF64_ERR_TRUNCATED;
    }
    if (p->memsz != 0 && add_overflows(p->vaddr, p->memsz)) {
        return JOSH_ELF64_ERR_SEGMENT;
    }
    if (p->align > 1u) {
        if (!power_of_two(p->align)) return JOSH_ELF64_ERR_SEGMENT;
        if ((p->vaddr & (p->align - 1u)) !=
            (p->offset & (p->align - 1u))) {
            return JOSH_ELF64_ERR_SEGMENT;
        }
    }
    return JOSH_ELF64_OK;
}

JoshElf64Status josh_elf64_validate(
    const void *image,
    je_u64 image_size,
    JoshElf64Summary *summary
) {
    if (!summary) return JOSH_ELF64_ERR_ARGUMENT;

    JoshElf64Status status = JOSH_ELF64_OK;
    const Elf64Header *h = header_if_basic_valid(image, image_size, &status);
    if (!h) return status;

    je_u64 virtual_min = ~((je_u64)0);
    je_u64 virtual_max = 0;
    je_u16 load_count = 0;
    int entry_is_executable = 0;

    for (je_u16 i = 0; i < h->phnum; ++i) {
        const Elf64ProgramHeader *p = program_header(image, h, i);
        if (p->type != JOSH_ELF64_PT_LOAD) continue;

        status = validate_load_segment(p, image_size);
        if (status != JOSH_ELF64_OK) return status;
        if (p->memsz == 0) continue;

        je_u64 end = p->vaddr + p->memsz;
        if (p->vaddr < virtual_min) virtual_min = p->vaddr;
        if (end > virtual_max) virtual_max = end;
        ++load_count;

        if ((p->flags & JOSH_ELF64_PF_X) != 0 &&
            h->entry >= p->vaddr && h->entry < end) {
            entry_is_executable = 1;
        }

        for (je_u16 j = 0; j < i; ++j) {
            const Elf64ProgramHeader *q = program_header(image, h, j);
            if (q->type != JOSH_ELF64_PT_LOAD || q->memsz == 0) continue;
            je_u64 q_end = q->vaddr + q->memsz;
            if (p->vaddr < q_end && q->vaddr < end) {
                return JOSH_ELF64_ERR_OVERLAP;
            }
        }
    }

    if (load_count == 0) return JOSH_ELF64_ERR_SEGMENT;
    if (!entry_is_executable) return JOSH_ELF64_ERR_ENTRY;

    summary->entry = h->entry;
    summary->virtual_min = virtual_min;
    summary->virtual_max = virtual_max;
    summary->load_segment_count = load_count;
    return JOSH_ELF64_OK;
}

JoshElf64Status josh_elf64_load_segment(
    const void *image,
    je_u64 image_size,
    je_u16 load_index,
    JoshElf64LoadSegment *segment
) {
    if (!segment) return JOSH_ELF64_ERR_ARGUMENT;

    JoshElf64Status status = JOSH_ELF64_OK;
    const Elf64Header *h = header_if_basic_valid(image, image_size, &status);
    if (!h) return status;

    je_u16 current = 0;
    for (je_u16 i = 0; i < h->phnum; ++i) {
        const Elf64ProgramHeader *p = program_header(image, h, i);
        if (p->type != JOSH_ELF64_PT_LOAD || p->memsz == 0) continue;

        status = validate_load_segment(p, image_size);
        if (status != JOSH_ELF64_OK) return status;
        if (current++ != load_index) continue;

        segment->file_offset = p->offset;
        segment->virtual_address = p->vaddr;
        segment->file_size = p->filesz;
        segment->memory_size = p->memsz;
        segment->alignment = p->align;
        segment->flags = p->flags;
        return JOSH_ELF64_OK;
    }

    return JOSH_ELF64_ERR_SEGMENT;
}

const char *josh_elf64_status_string(JoshElf64Status status) {
    switch (status) {
        case JOSH_ELF64_OK: return "ok";
        case JOSH_ELF64_ERR_ARGUMENT: return "invalid argument";
        case JOSH_ELF64_ERR_TRUNCATED: return "truncated image";
        case JOSH_ELF64_ERR_MAGIC: return "bad ELF magic";
        case JOSH_ELF64_ERR_CLASS: return "not ELF64";
        case JOSH_ELF64_ERR_ENDIAN: return "not little endian";
        case JOSH_ELF64_ERR_VERSION: return "unsupported ELF version";
        case JOSH_ELF64_ERR_TYPE: return "not executable ELF";
        case JOSH_ELF64_ERR_MACHINE: return "not x86-64";
        case JOSH_ELF64_ERR_PHDR: return "invalid program headers";
        case JOSH_ELF64_ERR_SEGMENT: return "invalid load segment";
        case JOSH_ELF64_ERR_OVERLAP: return "overlapping load segments";
        case JOSH_ELF64_ERR_ENTRY: return "entry outside executable segment";
        default: return "unknown ELF error";
    }
}
