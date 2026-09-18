/*
 * ata.h - ATA PIO block device for the Josh boot stack. See ata.c.
 */
#ifndef JOSH_ATA_H
#define JOSH_ATA_H

#include <stdint.h>
#include "fat32.h"      /* for josh_blockdev */

#define ATA_OK            0
#define ATA_E_TIMEOUT    -1
#define ATA_E_ERR        -2
#define ATA_E_FAULT      -3
#define ATA_E_NO_DEVICE  -4
#define ATA_E_NOT_ATA    -5
#define ATA_E_RANGE      -6

/* Standard legacy IDE port pairs. */
#define ATA_PRIMARY_IO     0x1F0
#define ATA_PRIMARY_CTRL   0x3F6
#define ATA_SECONDARY_IO   0x170
#define ATA_SECONDARY_CTRL 0x376

typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t  slave;
    uint8_t  present;
    uint8_t  lba48;
    uint32_t sectors;     /* LBA28 capacity */
    uint64_t sectors64;   /* LBA48 capacity if available */
} ata_device;

int  ata_init(ata_device *d, uint16_t io_base, uint16_t ctrl_base, int slave);
int  ata_identify(ata_device *d);
int  ata_read_sectors(ata_device *d, uint64_t lba, uint32_t count, void *buf);
void ata_as_blockdev(ata_device *d, josh_blockdev *out);
const char *ata_strerror(int e);

#endif /* JOSH_ATA_H */
