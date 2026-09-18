/*
 * elf64.c - Minimal ELF64 loader. See elf64.h.
 */
#include "elf64.h"

#define EI_NIDENT     16
#define ELFCLASS64    2
#define ELFDATA2LSB   1
#define EV_CURRENT    1
#define ET_EXEC       2
#define EM_X86_64     0x3E
#define PT_LOAD       1

#define EHDR_SIZE     64
#define PHDR_SIZE     56

/* Cap phnum so a corrupt header cannot spin us for minutes. The ELF spec
 * allows 65535; no sane kernel has more than a handful. */
#define MAX_PHNUM     64

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p) {
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

static void zero(void *d, uint64_t n) {
    uint8_t *p = (uint8_t *)d;
    while (n--) *p++ = 0;
}

const char *elf64_strerror(elf64_err e) {
    switch (e) {
    case ELF64_OK:             return "ok";
    case ELF64_E_IO:           return "read failed";
    case ELF64_E_NOT_ELF:      return "not an ELF file";
    case ELF64_E_NOT_64BIT:    return "not ELF64";
    case ELF64_E_NOT_LE:       return "not little-endian";
    case ELF64_E_NOT_EXEC:     return "not ET_EXEC";
    case ELF64_E_NOT_X86_64:   return "not x86-64";
    case ELF64_E_BAD_PHDR:     return "bad program header table";
    case ELF64_E_BAD_SEGMENT:  return "bad segment";
    case ELF64_E_NO_SEGMENTS:  return "no loadable segments";
    case ELF64_E_OUT_OF_RANGE: return "segment outside permitted range";
    case ELF64_E_INVAL:        return "invalid argument";
    }
    return "unknown error";
}

static elf64_err read_ehdr(elf64_read_fn read, void *ctx, uint8_t *eh) {
    if (read(ctx, 0, eh, EHDR_SIZE) != 0) return ELF64_E_IO;

    if (eh[0] != 0x7F || eh[1] != 'E' || eh[2] != 'L' || eh[3] != 'F')
        return ELF64_E_NOT_ELF;
    if (eh[4] != ELFCLASS64)  return ELF64_E_NOT_64BIT;
    if (eh[5] != ELFDATA2LSB) return ELF64_E_NOT_LE;
    if (eh[6] != EV_CURRENT)  return ELF64_E_NOT_ELF;

    if (rd16(&eh[16]) != ET_EXEC)   return ELF64_E_NOT_EXEC;
    if (rd16(&eh[18]) != EM_X86_64) return ELF64_E_NOT_X86_64;

    if (rd16(&eh[54]) < PHDR_SIZE) return ELF64_E_BAD_PHDR;
    if (rd16(&eh[56]) == 0)        return ELF64_E_NO_SEGMENTS;
    if (rd16(&eh[56]) > MAX_PHNUM) return ELF64_E_BAD_PHDR;
    if (rd64(&eh[32]) == 0)        return ELF64_E_BAD_PHDR;

    return ELF64_OK;
}

elf64_err elf64_probe(elf64_read_fn read, void *ctx, elf64_image *out) {
    uint8_t eh[EHDR_SIZE];
    elf64_err e;

    if (!read || !out) return ELF64_E_INVAL;
    e = read_ehdr(read, ctx, eh);
    if (e != ELF64_OK) return e;

    out->entry         = rd64(&eh[24]);
    out->lowest_paddr  = 0;
    out->highest_paddr = 0;
    out->segments      = 0;
    return ELF64_OK;
}

elf64_err elf64_load(elf64_read_fn read, void *ctx,
                     uint64_t min_paddr, uint64_t max_paddr,
                     elf64_image *out) {
    uint8_t  eh[EHDR_SIZE];
    uint8_t  ph[PHDR_SIZE];
    uint64_t phoff;
    uint16_t phentsize, phnum, i;
    elf64_err e;
    int loaded = 0;

    if (!read || !out) return ELF64_E_INVAL;
    if (min_paddr >= max_paddr) return ELF64_E_INVAL;

    e = read_ehdr(read, ctx, eh);
    if (e != ELF64_OK) return e;

    phoff     = rd64(&eh[32]);
    phentsize = rd16(&eh[54]);
    phnum     = rd16(&eh[56]);

    out->entry         = rd64(&eh[24]);
    out->lowest_paddr  = 0xFFFFFFFFFFFFFFFFull;
    out->highest_paddr = 0;
    out->segments      = 0;

    for (i = 0; i < phnum; i++) {
        uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, end;
        uint32_t p_type;

        if (read(ctx, phoff + (uint64_t)i * phentsize, ph, PHDR_SIZE) != 0)
            return ELF64_E_IO;

        p_type = rd32(&ph[0]);
        if (p_type != PT_LOAD) continue;

        p_offset = rd64(&ph[8]);
        p_vaddr  = rd64(&ph[16]);
        p_paddr  = rd64(&ph[24]);
        p_filesz = rd64(&ph[32]);
        p_memsz  = rd64(&ph[40]);

        if (p_memsz == 0) continue;
        if (p_filesz > p_memsz) return ELF64_E_BAD_SEGMENT;

        if (p_paddr == 0 && p_vaddr != 0) p_paddr = p_vaddr;

        if (p_paddr < min_paddr) return ELF64_E_OUT_OF_RANGE;
        if (p_memsz > max_paddr - p_paddr) return ELF64_E_OUT_OF_RANGE;
        end = p_paddr + p_memsz;

        if (p_filesz > 0) {
            uint64_t done = 0;
            while (done < p_filesz) {
                uint32_t chunk = 0x10000;
                if ((uint64_t)chunk > p_filesz - done)
                    chunk = (uint32_t)(p_filesz - done);
                if (read(ctx, p_offset + done,
                         (void *)(uintptr_t)(p_paddr + done), chunk) != 0)
                    return ELF64_E_IO;
                done += chunk;
            }
        }

        if (p_memsz > p_filesz)
            zero((void *)(uintptr_t)(p_paddr + p_filesz), p_memsz - p_filesz);

        if (p_paddr < out->lowest_paddr) out->lowest_paddr = p_paddr;
        if (end     > out->highest_paddr) out->highest_paddr = end;
        out->segments++;
        loaded = 1;
    }

    if (!loaded) {
        out->lowest_paddr = 0;
        return ELF64_E_NO_SEGMENTS;
    }
    return ELF64_OK;
}
