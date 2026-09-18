#define _GNU_SOURCE
/*
 * test_elf64.c - Host tests for the ELF64 loader.
 *
 * Loads a real ELF64 binary produced by the system linker into a
 * simulated physical memory buffer, then checks the bytes landed where
 * the program headers said they should, and that .bss was zeroed.
 *
 * Also feeds the loader deliberately malformed headers. A bootloader
 * that trusts its input is a bootloader that hangs with no output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "../boot/elf64.h"

static int tests_run = 0, tests_failed = 0;

#define CHECK(cond, ...) do {                                    \
    tests_run++;                                                 \
    if (!(cond)) {                                               \
        tests_failed++;                                          \
        printf("  FAIL %s:%d: ", __FILE__, __LINE__);            \
        printf(__VA_ARGS__);                                     \
        printf("\n");                                            \
    }                                                            \
} while (0)

/* ---------------------------------------------------------------------
 * Simulated physical memory.
 *
 * The loader writes to absolute physical addresses, including from its
 * .bss-zeroing path which does not go through the read callback. Rather
 * than add a memset hook purely for testability, we mmap real pages at
 * the address the fixture is linked to. The loader then runs completely
 * unmodified and we still observe every byte it writes.
 * ------------------------------------------------------------------- */
#define SIM_BASE  0x100000ull          /* fixture is linked here         */
#define SIM_SIZE  (32u * 1024 * 1024)

static uint8_t *sim_mem;

static int sim_map(void) {
    void *p = mmap((void *)SIM_BASE, SIM_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) return 1;
    sim_mem = (uint8_t *)p;
    return 0;
}

typedef struct {
    uint8_t *data;
    size_t   size;
    int      fail_after;   /* -1 = never fail */
    int      reads;
} file_ctx;

static int file_read(void *ctx, uint64_t offset, void *buf, uint32_t len) {
    file_ctx *f = (file_ctx *)ctx;

    if (f->fail_after >= 0 && f->reads >= f->fail_after) return 1;
    f->reads++;

    if (offset > f->size || len > f->size - offset) return 1;
    memcpy(buf, f->data + offset, len);
    return 0;
}

static uint8_t *slurp(const char *path, size_t *out_size) {
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END); n = ftell(fp); fseek(fp, 0, SEEK_SET);
    buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { free(buf); fclose(fp); return NULL; }
    fclose(fp);
    *out_size = (size_t)n;
    return buf;
}

/* --------------------------------------------------------------- tests */

static void test_real_binary(const char *path) {
    file_ctx ctx;
    elf64_image img;
    elf64_err e;
    size_t size;
    uint8_t *data = slurp(path, &size);

    printf("load a real linked ELF64 kernel (%s)\n", path);
    if (!data) { printf("  (skip: cannot open)\n"); return; }

    ctx.data = data; ctx.size = size; ctx.fail_after = -1; ctx.reads = 0;

    e = elf64_probe(file_read, &ctx, &img);
    CHECK(e == ELF64_OK, "probe: %s", elf64_strerror(e));
    CHECK(img.entry != 0, "entry point is zero");

    memset(sim_mem, 0xCC, SIM_SIZE);   /* poison, so .bss zeroing shows */
    ctx.reads = 0;

    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_OK, "load: %s", elf64_strerror(e));
    if (e != ELF64_OK) { free(data); return; }

    CHECK(img.segments >= 1, "segments = %u", img.segments);
    CHECK(img.highest_paddr > img.lowest_paddr, "empty address range");
    CHECK(img.lowest_paddr >= SIM_BASE, "lowest below window");
    printf("  entry=0x%llx range=[0x%llx,0x%llx) segments=%u\n",
           (unsigned long long)img.entry,
           (unsigned long long)img.lowest_paddr,
           (unsigned long long)img.highest_paddr, img.segments);

    /* The linker script places a known marker at the load address.
     * Check the first bytes of .text actually arrived. */
    {
        uint8_t *p = (uint8_t *)(uintptr_t)img.lowest_paddr;
        int all_poison = 1, i;
        for (i = 0; i < 64; i++) if (p[i] != 0xCC) { all_poison = 0; break; }
        CHECK(!all_poison, "load address still poisoned, nothing was written");
    }
    free(data);
}

static void test_bss_zeroed(const char *path) {
    file_ctx ctx;
    elf64_image img;
    size_t size;
    uint8_t *data = slurp(path, &size);

    printf(".bss is zeroed (memsz > filesz)\n");
    if (!data) { printf("  (skip)\n"); return; }

    ctx.data = data; ctx.size = size; ctx.fail_after = -1; ctx.reads = 0;
    memset(sim_mem, 0xCC, SIM_SIZE);

    if (elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img)
        == ELF64_OK) {
        /* The test kernel declares a 4 KiB zero-initialised array. Scan
         * the tail of the loaded range for a run of zeroes that the
         * poison pattern could not have produced. */
        uint8_t *p = (uint8_t *)(uintptr_t)img.lowest_paddr;
        uint64_t span = img.highest_paddr - img.lowest_paddr;
        uint64_t i, run = 0, best = 0;
        for (i = 0; i < span; i++) {
            if (p[i] == 0) {
                if (++run > best) best = run;
            } else run = 0;
        }
        CHECK(best >= 4096, "longest zero run = %llu, expected >= 4096 "
              "(.bss not zeroed?)", (unsigned long long)best);
    }
    free(data);
}

static void test_rejects_garbage(void) {
    file_ctx ctx;
    elf64_image img;
    static uint8_t buf[4096];
    elf64_err e;

    printf("malformed input is rejected\n");

    /* Not ELF at all. */
    memset(buf, 0xAB, sizeof(buf));
    ctx.data = buf; ctx.size = sizeof(buf); ctx.fail_after = -1; ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_NOT_ELF, "garbage returned %s", elf64_strerror(e));

    /* Valid magic, 32-bit class. */
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x7F; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 1;    /* ELFCLASS32 */
    buf[5] = 1; buf[6] = 1;
    ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_NOT_64BIT, "ELF32 returned %s", elf64_strerror(e));

    /* ELF64 little-endian but big-endian data flag. */
    buf[4] = 2; buf[5] = 2;
    ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_NOT_LE, "big-endian returned %s", elf64_strerror(e));

    /* Correct ident, wrong machine. */
    buf[5] = 1;
    buf[16] = 2; buf[17] = 0;        /* ET_EXEC        */
    buf[18] = 0x28; buf[19] = 0;     /* EM_ARM         */
    ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_NOT_X86_64, "ARM returned %s", elf64_strerror(e));

    /* Right machine, absurd phnum. */
    buf[18] = 0x3E; buf[19] = 0;
    buf[32] = 64;                    /* phoff = 64     */
    buf[54] = 56; buf[55] = 0;       /* phentsize      */
    buf[56] = 0xFF; buf[57] = 0xFF;  /* phnum = 65535  */
    ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_BAD_PHDR, "huge phnum returned %s", elf64_strerror(e));
}

static void test_range_enforcement(void) {
    static uint8_t buf[4096];
    file_ctx ctx;
    elf64_image img;
    elf64_err e;

    printf("segments outside the permitted window are refused\n");

    memset(buf, 0, sizeof(buf));
    buf[0] = 0x7F; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
    buf[4] = 2; buf[5] = 1; buf[6] = 1;
    buf[16] = 2;                      /* ET_EXEC       */
    buf[18] = 0x3E;                   /* EM_X86_64     */
    buf[32] = 64;                     /* phoff         */
    buf[54] = 56;                     /* phentsize     */
    buf[56] = 1;                      /* phnum = 1     */

    {
        uint8_t *ph = buf + 64;
        ph[0] = 1;                    /* PT_LOAD       */
        /* p_paddr = 0x1000: below the window. */
        ph[24] = 0x00; ph[25] = 0x10;
        ph[40] = 0x00; ph[41] = 0x10; /* memsz = 0x1000 */
    }
    ctx.data = buf; ctx.size = sizeof(buf); ctx.fail_after = -1; ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_OUT_OF_RANGE, "low segment returned %s",
          elf64_strerror(e));

    printf("integer overflow in the range check does not wrap\n");
    {
        uint8_t *ph = buf + 64;
        int i;
        /* p_paddr just inside the window, memsz = 0xFFFFFFFFFFFFF000.
         * A naive (paddr + memsz > max) check wraps and passes. */
        for (i = 0; i < 8; i++) ph[24 + i] = 0;
        ph[24] = 0x00; ph[25] = 0x00; ph[26] = 0x10;   /* 0x100000 */
        for (i = 0; i < 8; i++) ph[40 + i] = 0xFF;
        ph[40] = 0x00;
    }
    ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_OUT_OF_RANGE, "overflow segment returned %s",
          elf64_strerror(e));

    printf("filesz > memsz is refused\n");
    {
        uint8_t *ph = buf + 64;
        int i;
        for (i = 0; i < 8; i++) { ph[32 + i] = 0; ph[40 + i] = 0; }
        ph[32] = 0x00; ph[33] = 0x20;   /* filesz = 0x2000 */
        ph[40] = 0x00; ph[41] = 0x10;   /* memsz  = 0x1000 */
    }
    ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_BAD_SEGMENT, "filesz>memsz returned %s",
          elf64_strerror(e));
}

static void test_io_failure(const char *path) {
    file_ctx ctx;
    elf64_image img;
    elf64_err e;
    size_t size;
    uint8_t *data = slurp(path, &size);

    printf("read failure mid-load is reported, not ignored\n");
    if (!data) { printf("  (skip)\n"); return; }

    ctx.data = data; ctx.size = size; ctx.fail_after = 0; ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_IO, "immediate failure returned %s",
          elf64_strerror(e));

    ctx.fail_after = 2; ctx.reads = 0;
    e = elf64_load(file_read, &ctx, SIM_BASE, SIM_BASE + SIM_SIZE, &img);
    CHECK(e == ELF64_E_IO, "mid-load failure returned %s",
          elf64_strerror(e));
    free(data);
}

int main(int argc, char **argv) {
    const char *kernel = argc > 1 ? argv[1] : "build/testkernel.elf";

    printf("=== ELF64 loader tests ===\n\n");
    if (sim_map() != 0) {
        printf("FATAL: cannot map simulated physical memory at 0x%llx\n",
               (unsigned long long)SIM_BASE);
        return 2;
    }

    test_real_binary(kernel);
    test_bss_zeroed(kernel);
    test_rejects_garbage();
    test_range_enforcement();
    test_io_failure(kernel);

    munmap(sim_mem, SIM_SIZE);
    printf("\n=== %d checks, %d failed ===\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
