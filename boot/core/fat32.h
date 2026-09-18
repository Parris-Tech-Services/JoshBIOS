#ifndef JOSHBOOT_FAT32_H
#define JOSHBOOT_FAT32_H

#include <stddef.h>
#include <stdint.h>

#include "block.h"
#include "partition.h"

typedef enum {
    JOSH_FAT32_OK = 0,
    JOSH_FAT32_IO_ERROR,
    JOSH_FAT32_NOT_FAT32,
    JOSH_FAT32_CORRUPT,
    JOSH_FAT32_UNSUPPORTED,
    JOSH_FAT32_NOT_FOUND,
    JOSH_FAT32_NOT_DIRECTORY,
    JOSH_FAT32_BAD_PATH,
    JOSH_FAT32_BUFFER_TOO_SMALL
} josh_fat32_status_t;

typedef struct {
    const josh_block_device_t *device;
    uint64_t partition_lba;
    uint64_t partition_sectors;
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t sectors_per_fat;
    uint32_t cluster_count;
    uint32_t root_cluster;
    uint8_t sectors_per_cluster;
    uint8_t fat_count;
} josh_fat32_t;

typedef struct {
    uint32_t first_cluster;
    uint32_t size;
    uint8_t attributes;
} josh_fat32_file_t;

josh_fat32_status_t josh_fat32_mount(const josh_block_device_t *device,
                                      const josh_partition_t *partition,
                                      josh_fat32_t *filesystem);
josh_fat32_status_t josh_fat32_open_path(const josh_fat32_t *filesystem,
                                          const char *path,
                                          josh_fat32_file_t *file);
josh_fat32_status_t josh_fat32_read_file(const josh_fat32_t *filesystem,
                                          const josh_fat32_file_t *file,
                                          uint32_t offset,
                                          void *buffer,
                                          uint32_t length,
                                          uint32_t *bytes_read);
const char *josh_fat32_status_string(josh_fat32_status_t status);

#endif
