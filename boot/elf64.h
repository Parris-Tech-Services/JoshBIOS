#ifndef JOSHBOOT_ELF64_H
#define JOSHBOOT_ELF64_H

typedef unsigned char je_u8;
typedef unsigned short je_u16;
typedef unsigned int je_u32;
typedef unsigned long long je_u64;

#define JOSH_ELF64_MAX_PHDRS 128u
#define JOSH_ELF64_PT_LOAD 1u
#define JOSH_ELF64_PF_X 1u

typedef enum {
    JOSH_ELF64_OK = 0,
    JOSH_ELF64_ERR_ARGUMENT,
    JOSH_ELF64_ERR_TRUNCATED,
    JOSH_ELF64_ERR_MAGIC,
    JOSH_ELF64_ERR_CLASS,
    JOSH_ELF64_ERR_ENDIAN,
    JOSH_ELF64_ERR_VERSION,
    JOSH_ELF64_ERR_TYPE,
    JOSH_ELF64_ERR_MACHINE,
    JOSH_ELF64_ERR_PHDR,
    JOSH_ELF64_ERR_SEGMENT,
    JOSH_ELF64_ERR_OVERLAP,
    JOSH_ELF64_ERR_ENTRY
} JoshElf64Status;

typedef struct {
    je_u64 file_offset;
    je_u64 virtual_address;
    je_u64 file_size;
    je_u64 memory_size;
    je_u64 alignment;
    je_u32 flags;
} JoshElf64LoadSegment;

typedef struct {
    je_u64 entry;
    je_u64 virtual_min;
    je_u64 virtual_max;
    je_u16 load_segment_count;
} JoshElf64Summary;

JoshElf64Status josh_elf64_validate(const void *image, je_u64 image_size,
                                     JoshElf64Summary *summary);
JoshElf64Status josh_elf64_load_segment(const void *image, je_u64 image_size,
                                         je_u16 load_index,
                                         JoshElf64LoadSegment *segment);
const char *josh_elf64_status_string(JoshElf64Status status);

#endif
