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
 * Supports: FAT32 only, read-only, MBR partitions, 8.3 + VFAT long names.
 * Does not support: FAT12/16, exFAT, writing, GPT (see BOOTLOADER_ROADMAP).
 */
#ifndef JOSH_FAT32_H
#define JOSH_FAT32_H

#include <stdint.h>

#define FAT32_MAX_PATH      256
#define FAT32_MAX_NAME      256
#define FAT32_SECTOR_SIZE   512

typedef enum {
    FAT32_OK              =  0,
    FAT32_E_IO            = -1,
    FAT32_E_NO_FAT32      = -2,
    FAT32_E_BAD_BPB       = -3,
    FAT32_E_NOT_FOUND     = -4,
    FAT32_E_NOT_A_FILE    = -5,
    FAT32_E_NOT_A_DIR     = -6,
    FAT32_E_BAD_CLUSTER   = -7,
    FAT32_E_TOO_BIG       = -8,
    FAT32_E_INVAL         = -9,
    FAT32_E_LOOP          = -10
} fat32_err;

typedef struct josh_blockdev {
    int  (*read)(struct josh_blockdev *dev, uint64_t lba,
                 uint32_t count, void *buf);
    void  *ctx;
    uint32_t sector_size;
} josh_blockdev;

typedef struct {
    josh_blockdev *dev;
    uint32_t partition_lba;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint32_t fat_start_lba;
    uint32_t fat_sectors;
    uint8_t  num_fats;
    uint32_t cluster_heap_lba;
    uint32_t root_cluster;
    uint32_t total_clusters;
    uint32_t bytes_per_cluster;
    uint8_t  fat_cache[FAT32_SECTOR_SIZE];
    uint32_t fat_cache_lba;
} fat32_volume;

typedef struct {
    char     name[FAT32_MAX_NAME];
    char     short_name[13];
    uint32_t first_cluster;
    uint32_t size;
    uint8_t  attr;
    uint8_t  is_dir;
} fat32_dirent;

typedef struct {
    fat32_volume *vol;
    uint32_t cluster;
    uint32_t sector_in_cluster;
    uint32_t offset_in_sector;
    uint8_t  sector[FAT32_SECTOR_SIZE];
    int      sector_loaded;
    uint32_t guard;
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

fat32_err fat32_mount(fat32_volume *vol, josh_blockdev *dev);
fat32_err fat32_mount_at(fat32_volume *vol, josh_blockdev *dev,
                         uint32_t partition_lba);
fat32_err fat32_stat(fat32_volume *vol, const char *path, fat32_dirent *out);
fat32_err fat32_read_file(fat32_volume *vol, const char *path,
                          void *buf, uint32_t buf_size, uint32_t *out_size);
fat32_err fat32_read_at(fat32_volume *vol, const fat32_dirent *ent,
                        uint32_t offset, void *buf, uint32_t len);
fat32_err fat32_opendir(fat32_volume *vol, const char *path, fat32_dir *dir);
fat32_err fat32_readdir(fat32_dir *dir, fat32_dirent *out);
fat32_err fat32_next_cluster(fat32_volume *vol, uint32_t cluster,
                             uint32_t *next);
const char *fat32_strerror(fat32_err e);

#endif
