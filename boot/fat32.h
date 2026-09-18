/*
 * fat32.h - Read-only FAT32 driver for the Josh boot stack.
 *
 * Design notes
 * ------------
 * The driver never touches hardware directly. It reads through a
 * josh_blockdev vtable, so the same code runs:
 *
 *   - in the bootloader, backed by ATA PIO      (boot/ata.c)
 *   - on a Linux host, backed by a file         (tests/host_blockdev.c)
 *
 * That is deliberate: filesystem bugs are the kind that vanish silently
 * and reappear at 3am on real hardware. Being able to run the exact same
 * parser against a mkfs.vfat image under a host test harness is worth
 * more than any amount of careful reading.
 *
 * Supports: FAT32 only, read-only, MBR partitions, 8.3 + VFAT long names.
 * Does not support: FAT12/16, exFAT, writing, GPT (see BOOTLOADER_ROADMAP).
 */
#ifndef JOSH_FAT32_H
#define JOSH_FAT32_H

#include <stdint.h>

#define FAT32_MAX_PATH      256
#define FAT32_MAX_NAME      256
#define FAT32_SECTOR_SIZE   512

/* Error codes. Negative so callers can test < 0. */
typedef enum {
    FAT32_OK              =  0,
    FAT32_E_IO            = -1,   /* block device read failed            */
    FAT32_E_NO_FAT32      = -2,   /* not a FAT32 volume                  */
    FAT32_E_BAD_BPB       = -3,   /* BPB field out of range              */
    FAT32_E_NOT_FOUND     = -4,   /* path does not exist                 */
    FAT32_E_NOT_A_FILE    = -5,   /* expected file, found directory      */
    FAT32_E_NOT_A_DIR     = -6,   /* expected directory, found file      */
    FAT32_E_BAD_CLUSTER   = -7,   /* cluster chain corrupt / bad marker   */
    FAT32_E_TOO_BIG       = -8,   /* file larger than caller's buffer    */
    FAT32_E_INVAL         = -9,   /* bad argument                        */
    FAT32_E_LOOP          = -10   /* cluster chain loops                 */
} fat32_err;

/* Block device abstraction. read() returns 0 on success, non-zero on error. */
typedef struct josh_blockdev {
    int  (*read)(struct josh_blockdev *dev, uint64_t lba,
                 uint32_t count, void *buf);
    void  *ctx;
    uint32_t sector_size;
} josh_blockdev;

typedef struct {
    josh_blockdev *dev;

    uint32_t partition_lba;      /* absolute LBA of the volume start     */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint32_t fat_start_lba;      /* absolute                             */
    uint32_t fat_sectors;
    uint8_t  num_fats;
    uint32_t cluster_heap_lba;   /* absolute LBA of cluster 2            */
    uint32_t root_cluster;
    uint32_t total_clusters;     /* count of valid data clusters         */
    uint32_t bytes_per_cluster;

    /* Single-sector FAT cache. The FAT is walked one entry at a time and
     * consecutive clusters almost always live in the same sector, so even
     * this trivial cache removes most of the reads. */
    uint8_t  fat_cache[FAT32_SECTOR_SIZE];
    uint32_t fat_cache_lba;      /* 0 = empty                            */
} fat32_volume;

typedef struct {
    char     name[FAT32_MAX_NAME]; /* long name if present, else 8.3     */
    char     short_name[13];       /* always the 8.3 form                */
    uint32_t first_cluster;
    uint32_t size;
    uint8_t  attr;
    uint8_t  is_dir;
} fat32_dirent;

/* Directory iteration state. */
typedef struct {
    fat32_volume *vol;
    uint32_t cluster;
    uint32_t sector_in_cluster;
    uint32_t offset_in_sector;
    uint8_t  sector[FAT32_SECTOR_SIZE];
    int      sector_loaded;
    uint32_t guard;              /* cluster-hop counter, loop defence    */
    /* LFN assembly */
    char     lfn[FAT32_MAX_NAME];
    int      lfn_valid;
    uint8_t  lfn_checksum;
} fat32_dir;

#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LFN        0x0F

/* Mount the first FAT32 partition found in the MBR, or the whole device
 * if it is a superfloppy (no MBR partition table). */
fat32_err fat32_mount(fat32_volume *vol, josh_blockdev *dev);

/* Mount a volume whose start LBA is already known. */
fat32_err fat32_mount_at(fat32_volume *vol, josh_blockdev *dev,
                         uint32_t partition_lba);

/* Resolve a path such as "/BOOT/JOSH.ELF". Case-insensitive.
 * Accepts both '/' and '\\' separators. */
fat32_err fat32_stat(fat32_volume *vol, const char *path, fat32_dirent *out);

/* Read up to buf_size bytes of a file. *out_size receives the real size.
 * Returns FAT32_E_TOO_BIG if the file does not fit, but still sets
 * *out_size so the caller can allocate and retry. */
fat32_err fat32_read_file(fat32_volume *vol, const char *path,
                          void *buf, uint32_t buf_size, uint32_t *out_size);

/* Read from an already-resolved dirent, at an offset. Used by the ELF
 * loader to pull program headers without re-walking the path. */
fat32_err fat32_read_at(fat32_volume *vol, const fat32_dirent *ent,
                        uint32_t offset, void *buf, uint32_t len);

/* Directory iteration. fat32_readdir returns FAT32_OK and sets *out for
 * each entry, or FAT32_E_NOT_FOUND once the directory is exhausted. */
fat32_err fat32_opendir(fat32_volume *vol, const char *path, fat32_dir *dir);
fat32_err fat32_readdir(fat32_dir *dir, fat32_dirent *out);

/* Walk one link of a cluster chain. Exposed for diagnostics. */
fat32_err fat32_next_cluster(fat32_volume *vol, uint32_t cluster,
                             uint32_t *next);

const char *fat32_strerror(fat32_err e);

#endif /* JOSH_FAT32_H */
