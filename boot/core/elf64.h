#ifndef JOSHBOOT_ELF64_H
#define JOSHBOOT_ELF64_H

#include <stddef.h>
#include <stdint.h>

#define JOSH_ELF64_MAX_SEGMENTS 16u
#define JOSH_ELF64_PAGE_SIZE 4096u

typedef enum {
    JOSH_ELF64_OK = 0,
    JOSH_ELF64_TOO_SMALL,
    JOSH_ELF64_BAD_MAGIC,
    JOSH_ELF64_UNSUPPORTED_CLASS,
    JOSH_ELF64_UNSUPPORTED_ENDIAN,
    JOSH_ELF64_UNSUPPORTED_TYPE,
    JOSH_ELF64_UNSUPPORTED_MACHINE,
    JOSH_ELF64_CORRUPT,
    JOSH_ELF64_TOO_MANY_SEGMENTS,
    JOSH_ELF64_OVERLAP,
    JOSH_ELF64_UNSUPPORTED_ALIGNMENT,
    JOSH_ELF64_ENTRY_NOT_EXECUTABLE,
    JOSH_ELF64_OUT_OF_MEMORY,
    JOSH_ELF64_IO_ERROR
} josh_elf64_status_t;

typedef struct {
    uint64_t file_offset;
    uint64_t virtual_address;
    uint64_t original_physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
    uint32_t flags;
} josh_elf64_segment_t;

typedef struct {
    uint64_t entry;
    uint64_t virtual_min;
    uint64_t virtual_max;
    uint16_t segment_count;
    josh_elf64_segment_t segments[JOSH_ELF64_MAX_SEGMENTS];
} josh_elf64_image_t;

typedef struct {
    uint64_t virtual_page;
    uint64_t physical_page;
    uint64_t physical_address;
    uint64_t page_count;
    uint64_t file_offset;
    uint64_t file_size;
    uint64_t memory_size;
    uint32_t flags;
} josh_elf64_load_segment_t;

typedef struct {
    uint64_t entry;
    uint64_t physical_start;
    uint64_t physical_end;
    uint16_t segment_count;
    josh_elf64_load_segment_t segments[JOSH_ELF64_MAX_SEGMENTS];
} josh_elf64_load_plan_t;

typedef int (*josh_elf64_write_fn)(void *context, uint64_t physical_address,
                                    const void *data, size_t length);
typedef int (*josh_elf64_zero_fn)(void *context, uint64_t physical_address,
                                   size_t length);

josh_elf64_status_t josh_elf64_parse(const void *image, size_t image_size,
                                      josh_elf64_image_t *elf);
josh_elf64_status_t josh_elf64_plan(const josh_elf64_image_t *elf,
                                     uint64_t physical_base,
                                     uint64_t physical_limit,
                                     josh_elf64_load_plan_t *plan);
josh_elf64_status_t josh_elf64_load(const void *image, size_t image_size,
                                     const josh_elf64_load_plan_t *plan,
                                     josh_elf64_write_fn write_fn,
                                     josh_elf64_zero_fn zero_fn,
                                     void *context);
const char *josh_elf64_status_string(josh_elf64_status_t status);

#endif
