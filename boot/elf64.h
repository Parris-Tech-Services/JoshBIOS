/*
 * elf64.h - Minimal ELF64 loader for the Josh boot stack.
 *
 * Loads a static ET_EXEC x86-64 kernel image. Like fat32.c this reads
 * through a callback rather than assuming the whole file is in memory,
 * so a large kernel does not need to be buffered twice.
 *
 * Deliberately does NOT support: dynamic linking, relocations, shared
 * objects, section headers. A bootloader needs program headers and an
 * entry point; everything else is a place for bugs to hide.
 */
#ifndef JOSH_ELF64_H
#define JOSH_ELF64_H

#include <stdint.h>

typedef enum {
    ELF64_OK            =  0,
    ELF64_E_IO          = -1,   /* read callback failed                 */
    ELF64_E_NOT_ELF     = -2,   /* bad magic                            */
    ELF64_E_NOT_64BIT   = -3,   /* EI_CLASS != ELFCLASS64               */
    ELF64_E_NOT_LE      = -4,   /* EI_DATA != ELFDATA2LSB               */
    ELF64_E_NOT_EXEC    = -5,   /* e_type != ET_EXEC                    */
    ELF64_E_NOT_X86_64  = -6,   /* e_machine != EM_X86_64               */
    ELF64_E_BAD_PHDR    = -7,   /* phentsize/phnum implausible          */
    ELF64_E_BAD_SEGMENT = -8,   /* filesz > memsz, or wild address      */
    ELF64_E_NO_SEGMENTS = -9,   /* no PT_LOAD found                     */
    ELF64_E_OUT_OF_RANGE= -10,  /* segment outside the permitted window */
    ELF64_E_INVAL       = -11
} elf64_err;

/* Read len bytes at offset into buf. Return 0 on success. */
typedef int (*elf64_read_fn)(void *ctx, uint64_t offset,
                             void *buf, uint32_t len);

typedef struct {
    uint64_t entry;          /* virtual entry point                      */
    uint64_t lowest_paddr;   /* lowest physical address written          */
    uint64_t highest_paddr;  /* one past the highest physical byte       */
    uint32_t segments;       /* number of PT_LOAD segments loaded        */
} elf64_image;

/*
 * Load all PT_LOAD segments to their physical addresses.
 *
 * min_paddr / max_paddr bound where segments may be written. The
 * bootloader passes the region it knows is safe; a kernel that asks to
 * be loaded over the IVT or over the loader itself is rejected instead
 * of quietly corrupting memory and crashing somewhere unrelated later.
 *
 * memsz beyond filesz is zeroed, which is what .bss requires.
 */
elf64_err elf64_load(elf64_read_fn read, void *ctx,
                     uint64_t min_paddr, uint64_t max_paddr,
                     elf64_image *out);

/* Parse and validate headers without writing anything. */
elf64_err elf64_probe(elf64_read_fn read, void *ctx, elf64_image *out);

const char *elf64_strerror(elf64_err e);

#endif /* JOSH_ELF64_H */
