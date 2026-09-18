#ifndef JOSHBOOT_BLOCK_H
#define JOSHBOOT_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#define JOSH_BLOCK_SECTOR_SIZE 512u

typedef int (*josh_block_read_fn)(void *context, uint64_t lba, uint32_t sector_count, void *buffer);

typedef struct {
    void *context;
    uint64_t sector_count;
    josh_block_read_fn read;
} josh_block_device_t;

static inline int josh_block_read(const josh_block_device_t *device,
                                  uint64_t lba,
                                  uint32_t sector_count,
                                  void *buffer) {
    if (!device || !device->read || !buffer || sector_count == 0) return -1;
    if (lba >= device->sector_count) return -1;
    if ((uint64_t)sector_count > device->sector_count - lba) return -1;
    return device->read(device->context, lba, sector_count, buffer);
}

#endif
