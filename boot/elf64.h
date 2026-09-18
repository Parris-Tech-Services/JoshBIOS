/*
 * elf64.h - Minimal ELF64 loader for the Josh boot stack.
 */
#ifndef JOSH_ELF64_H
#define JOSH_ELF64_H
#include <stdint.h>
typedef enum {
    ELF64_OK=0, ELF64_E_IO=-1, ELF64_E_NOT_ELF=-2, ELF64_E_NOT_64BIT=-3,
    ELF64_E_NOT_LE=-4, ELF64_E_NOT_EXEC=-5, ELF64_E_NOT_X86_64=-6,
    ELF64_E_BAD_PHDR=-7, ELF64_E_BAD_SEGMENT=-8, ELF64_E_NO_SEGMENTS=-9,
    ELF64_E_OUT_OF_RANGE=-10, ELF64_E_INVAL=-11
} elf64_err;
typedef int (*elf64_read_fn)(void *ctx, uint64_t offset, void *buf, uint32_t len);
typedef struct {
    uint64_t entry;
    uint64_t lowest_paddr;
    uint64_t highest_paddr;
    uint32_t segments;
} elf64_image;
elf64_err elf64_load(elf64_read_fn read, void *ctx,
                     uint64_t min_paddr, uint64_t max_paddr,
                     elf64_image *out);
elf64_err elf64_probe(elf64_read_fn read, void *ctx, elf64_image *out);
const char *elf64_strerror(elf64_err e);
#endif
