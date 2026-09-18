#define _GNU_SOURCE
/*
 * test_fat32.c - Host-side tests for the FAT32 driver.
 *
 * Builds and runs on Linux against images produced by mkfs.vfat + mcopy,
 * so the parser is checked against filesystems made by a real, independent
 * implementation rather than by our own writer.
 *
 * Run with: make -C tests test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include "../boot/fat32.h"

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

/* ------------------------------------------------- file-backed blockdev */

typedef struct {
    FILE    *fp;
    uint64_t reads;        /* counted so we can assert on the FAT cache */
} host_ctx;

static int host_read(josh_blockdev *dev, uint64_t lba,
                     uint32_t count, void *buf) {
    host_ctx *c = (host_ctx *)dev->ctx;
    c->reads += count;
    if (fseeko(c->fp, (off_t)(lba * dev->sector_size), SEEK_SET) != 0)
        return 1;
    if (fread(buf, dev->sector_size, count, c->fp) != count)
        return 1;
    return 0;
}

/* A device that fails every read, to exercise the IO error paths. */
static int failing_read(josh_blockdev *dev, uint64_t lba,
                        uint32_t count, void *buf) {
    (void)dev; (void)lba; (void)count; (void)buf;
    return 1;
}

/* --------------------------------------------------------------- tests */

static void test_mount(fat32_volume *vol) {
    printf("mount and geometry\n");
    CHECK(vol->bytes_per_sector == 512,
          "bytes_per_sector = %u, want 512", vol->bytes_per_sector);
    CHECK(vol->sectors_per_cluster > 0,
          "sectors_per_cluster = %u", vol->sectors_per_cluster);
    CHECK(vol->root_cluster >= 2,
          "root_cluster = %u", vol->root_cluster);
    CHECK(vol->total_clusters >= 65525,
          "total_clusters = %u, must be >= 65525 for FAT32",
          vol->total_clusters);
    CHECK(vol->bytes_per_cluster ==
          (uint32_t)vol->bytes_per_sector * vol->sectors_per_cluster,
          "bytes_per_cluster inconsistent");
}

static void test_short_name(fat32_volume *vol) {
    char buf[128];
    uint32_t size = 0;
    fat32_err e;

    printf("8.3 short name in root\n");
    e = fat32_read_file(vol, "/HELLO.TXT", buf, sizeof(buf), &size);
    CHECK(e == FAT32_OK, "read /HELLO.TXT: %s", fat32_strerror(e));
    CHECK(size == 12, "size = %u, want 12", size);
    if (e == FAT32_OK)
        CHECK(memcmp(buf, "hello, josh\n", 12) == 0,
              "content mismatch: %.13s", buf);

    printf("case-insensitive lookup\n");
    e = fat32_read_file(vol, "/hello.txt", buf, sizeof(buf), &size);
    CHECK(e == FAT32_OK, "lowercase path: %s", fat32_strerror(e));

    printf("backslash separators\n");
    e = fat32_read_file(vol, "\\HELLO.TXT", buf, sizeof(buf), &size);
    CHECK(e == FAT32_OK, "backslash path: %s", fat32_strerror(e));
}

static void test_long_name(fat32_volume *vol) {
    char buf[128];
    uint32_t size = 0;
    fat32_err e;

    printf("VFAT long filename\n");
    e = fat32_read_file(vol, "/a rather long filename.txt",
                        buf, sizeof(buf), &size);
    CHECK(e == FAT32_OK, "read long name: %s", fat32_strerror(e));
    CHECK(size == 8, "size = %u, want 8", size);

    printf("very long filename (multi-fragment LFN)\n");
    e = fat32_read_file(vol,
        "/this-is-a-very-long-filename-that-needs-several-lfn-entries.txt",
        buf, sizeof(buf), &size);
    CHECK(e == FAT32_OK, "multi-fragment LFN: %s", fat32_strerror(e));
}

static void test_subdirs(fat32_volume *vol) {
    char buf[128];
    uint32_t size = 0;
    fat32_err e;

    printf("nested directories\n");
    e = fat32_read_file(vol, "/BOOT/nested/deep.txt", buf, sizeof(buf), &size);
    CHECK(e == FAT32_OK, "read nested: %s", fat32_strerror(e));
    CHECK(size == 5, "size = %u, want 5", size);

    printf("stat a directory\n");
    {
        fat32_dirent ent;
        e = fat32_stat(vol, "/BOOT", &ent);
        CHECK(e == FAT32_OK, "stat /BOOT: %s", fat32_strerror(e));
        CHECK(ent.is_dir == 1, "/BOOT is_dir = %d", ent.is_dir);

        e = fat32_stat(vol, "/", &ent);
        CHECK(e == FAT32_OK, "stat root: %s", fat32_strerror(e));
        CHECK(ent.is_dir == 1, "root is_dir = %d", ent.is_dir);
    }
}

static void test_large_file(fat32_volume *vol) {
    static uint8_t buf[1024 * 1024];
    uint32_t size = 0, i;
    fat32_err e;
    int bad = -1;

    printf("multi-cluster file (1 MiB, spans the FAT chain)\n");
    e = fat32_read_file(vol, "/BIG.BIN", buf, sizeof(buf), &size);
    CHECK(e == FAT32_OK, "read /BIG.BIN: %s", fat32_strerror(e));
    CHECK(size == 1024 * 1024, "size = %u, want 1048576", size);

    if (e == FAT32_OK) {
        /* The generator writes a repeating byte pattern i & 0xFF. */
        for (i = 0; i < size; i++) {
            if (buf[i] != (uint8_t)(i & 0xFF)) { bad = (int)i; break; }
        }
        CHECK(bad < 0, "content diverges at offset %d (got 0x%02x want 0x%02x)",
              bad, bad >= 0 ? buf[bad] : 0, bad >= 0 ? (bad & 0xFF) : 0);
    }
}

static void test_read_at(fat32_volume *vol) {
    fat32_dirent ent;
    uint8_t buf[4096];
    fat32_err e;
    uint32_t i;
    int bad = -1;

    printf("fat32_read_at offsets (ELF loader path)\n");
    e = fat32_stat(vol, "/BIG.BIN", &ent);
    CHECK(e == FAT32_OK, "stat BIG.BIN: %s", fat32_strerror(e));
    if (e != FAT32_OK) return;

    /* Read from a deliberately awkward offset: not sector-aligned, not
     * cluster-aligned, and crossing at least one cluster boundary. */
    e = fat32_read_at(vol, &ent, 5000, buf, 4096);
    CHECK(e == FAT32_OK, "read_at 5000: %s", fat32_strerror(e));
    if (e == FAT32_OK) {
        for (i = 0; i < 4096; i++) {
            if (buf[i] != (uint8_t)((5000 + i) & 0xFF)) {
                bad = (int)i; break;
            }
        }
        CHECK(bad < 0, "read_at content wrong at +%d", bad);
    }

    /* Offset past EOF must be rejected, not silently clamped. */
    e = fat32_read_at(vol, &ent, ent.size + 10, buf, 16);
    CHECK(e == FAT32_E_INVAL, "read past EOF returned %s",
          fat32_strerror(e));

    /* A read that straddles EOF is truncated, not an error. */
    e = fat32_read_at(vol, &ent, ent.size - 8, buf, 64);
    CHECK(e == FAT32_OK, "straddling read: %s", fat32_strerror(e));
}

static void test_readdir(fat32_volume *vol) {
    fat32_dir dir;
    fat32_dirent ent;
    int count = 0, saw_hello = 0, saw_boot = 0;
    fat32_err e;

    printf("directory iteration\n");
    e = fat32_opendir(vol, "/", &dir);
    CHECK(e == FAT32_OK, "opendir /: %s", fat32_strerror(e));

    while (fat32_readdir(&dir, &ent) == FAT32_OK) {
        count++;
        if (strcasecmp(ent.short_name, "HELLO.TXT") == 0) saw_hello = 1;
        if (strcasecmp(ent.short_name, "BOOT") == 0 && ent.is_dir) saw_boot = 1;
        if (count > 500) break;   /* runaway guard */
    }
    CHECK(count > 0 && count < 500, "root entry count = %d", count);
    CHECK(saw_hello, "HELLO.TXT not seen in root listing");
    CHECK(saw_boot,  "BOOT/ not seen in root listing");

    printf("opendir on a file is rejected\n");
    e = fat32_opendir(vol, "/HELLO.TXT", &dir);
    CHECK(e == FAT32_E_NOT_A_DIR, "opendir on file returned %s",
          fat32_strerror(e));
}

static void test_errors(fat32_volume *vol) {
    char small[4];
    uint32_t size = 0;
    fat32_err e;

    printf("error paths\n");

    e = fat32_read_file(vol, "/NOPE.TXT", small, sizeof(small), &size);
    CHECK(e == FAT32_E_NOT_FOUND, "missing file returned %s",
          fat32_strerror(e));

    e = fat32_read_file(vol, "/BOOT/alsonope", small, sizeof(small), &size);
    CHECK(e == FAT32_E_NOT_FOUND, "missing nested returned %s",
          fat32_strerror(e));

    /* Buffer too small must still report the true size. */
    size = 0;
    e = fat32_read_file(vol, "/HELLO.TXT", small, sizeof(small), &size);
    CHECK(e == FAT32_E_TOO_BIG, "small buffer returned %s",
          fat32_strerror(e));
    CHECK(size == 12, "size not reported on TOO_BIG: got %u", size);

    /* Treating a file as a directory. */
    e = fat32_read_file(vol, "/HELLO.TXT/inner", small, sizeof(small), &size);
    CHECK(e == FAT32_E_NOT_A_DIR || e == FAT32_E_NOT_FOUND,
          "file-as-dir returned %s", fat32_strerror(e));

    /* Reading a directory as a file. */
    e = fat32_read_file(vol, "/BOOT", small, sizeof(small), &size);
    CHECK(e == FAT32_E_NOT_A_FILE, "dir-as-file returned %s",
          fat32_strerror(e));

    /* NULL arguments must not crash. */
    CHECK(fat32_stat(NULL, "/x", NULL) == FAT32_E_INVAL, "NULL vol");
    CHECK(fat32_read_file(vol, NULL, small, 4, &size) == FAT32_E_INVAL,
          "NULL path");
}

static void test_io_failure(void) {
    josh_blockdev dev;
    fat32_volume  vol;
    fat32_err e;

    printf("block device failure is propagated\n");
    dev.read = failing_read;
    dev.ctx  = NULL;
    dev.sector_size = 512;

    e = fat32_mount(&vol, &dev);
    CHECK(e == FAT32_E_IO, "failing device returned %s", fat32_strerror(e));
}

static void test_garbage_bpb(const char *path) {
    /* Feed the mounter a device full of noise. It must refuse cleanly
     * rather than computing wild LBAs from a garbage BPB. */
    FILE *fp;
    josh_blockdev dev;
    host_ctx ctx;
    fat32_volume vol;
    fat32_err e;

    printf("garbage BPB is rejected\n");
    fp = fopen(path, "rb");
    if (!fp) { printf("  (skip: no garbage image)\n"); return; }

    ctx.fp = fp; ctx.reads = 0;
    dev.read = host_read; dev.ctx = &ctx; dev.sector_size = 512;

    e = fat32_mount(&vol, &dev);
    CHECK(e < 0, "garbage image mounted successfully (should not)");
    fclose(fp);
}

static void test_fat_cache(fat32_volume *vol, host_ctx *ctx) {
    static uint8_t buf[1024 * 1024];
    uint32_t size = 0;
    uint64_t before, after, data_sectors;

    printf("FAT cache reduces redundant reads\n");
    before = ctx->reads;
    fat32_read_file(vol, "/BIG.BIN", buf, sizeof(buf), &size);
    after  = ctx->reads;

    data_sectors = size / 512;
    /* Without a FAT cache, each cluster hop costs a FAT sector read. With
     * one, the FAT reads should be a small fraction of the data reads. */
    CHECK(after - before < data_sectors + (data_sectors / 4) + 32,
          "reads = %llu for %llu data sectors, cache not helping",
          (unsigned long long)(after - before),
          (unsigned long long)data_sectors);
}

int main(int argc, char **argv) {
    const char *img      = argc > 1 ? argv[1] : "build/test.img";
    const char *garbage  = argc > 2 ? argv[2] : "build/garbage.img";
    FILE *fp;
    josh_blockdev dev;
    host_ctx ctx;
    fat32_volume vol;
    fat32_err e;

    printf("=== FAT32 driver tests (image: %s) ===\n\n", img);

    fp = fopen(img, "rb");
    if (!fp) { perror(img); return 2; }

    ctx.fp = fp; ctx.reads = 0;
    dev.read = host_read; dev.ctx = &ctx; dev.sector_size = 512;

    e = fat32_mount(&vol, &dev);
    if (e != FAT32_OK) {
        printf("FATAL: mount failed: %s\n", fat32_strerror(e));
        return 2;
    }
    printf("mounted: partition_lba=%u spc=%u clusters=%u root=%u\n\n",
           vol.partition_lba, vol.sectors_per_cluster,
           vol.total_clusters, vol.root_cluster);

    test_mount(&vol);
    test_short_name(&vol);
    test_long_name(&vol);
    test_subdirs(&vol);
    test_large_file(&vol);
    test_read_at(&vol);
    test_readdir(&vol);
    test_errors(&vol);
    test_io_failure();
    test_garbage_bpb(garbage);
    test_fat_cache(&vol, &ctx);

    fclose(fp);

    printf("\n=== %d checks, %d failed ===\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
