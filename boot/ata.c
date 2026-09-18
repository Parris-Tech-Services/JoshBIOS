/*
 * ata.c - ATA PIO block device for the Josh boot stack.
 *
 * Once we leave real mode we lose INT 13h, so the loader needs its own
 * disk access. PIO is slow and unfashionable, and it is exactly right
 * here: no DMA setup, no interrupts, no PCI enumeration, works on every
 * IDE/SATA-in-compatibility-mode controller QEMU or a real board offers.
 *
 * Every wait loop has a bounded timeout. A bootloader that spins forever
 * on a status bit gives you a black screen and nothing to debug.
 */
#include "ata.h"

#define ATA_DATA        0x00
#define ATA_ERROR       0x01
#define ATA_SECCOUNT    0x02
#define ATA_LBA_LO      0x03
#define ATA_LBA_MID     0x04
#define ATA_LBA_HI      0x05
#define ATA_DRIVE       0x06
#define ATA_STATUS      0x07
#define ATA_COMMAND     0x07

#define ATA_SR_BSY      0x80
#define ATA_SR_DRDY     0x40
#define ATA_SR_DF       0x20
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01

#define CMD_READ_PIO    0x20
#define CMD_READ_PIO48  0x24
#define CMD_IDENTIFY    0xEC

/* Generous, but finite. Real spinning disks can take seconds to spin up. */
#define ATA_TIMEOUT     0x02000000u

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t r;
    __asm__ volatile ("inw %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

/* Reading the alternate status register takes ~100ns and has no side
 * effects. Four reads is the standard way to get the 400ns settle the
 * spec requires after a drive select. */
static void ata_delay400(ata_device *d) {
    int i;
    for (i = 0; i < 4; i++) (void)inb(d->ctrl_base);
}

static int ata_wait_not_busy(ata_device *d) {
    uint32_t spins = ATA_TIMEOUT;
    while (spins--) {
        uint8_t st = inb(d->io_base + ATA_STATUS);
        if (!(st & ATA_SR_BSY)) return 0;
    }
    return ATA_E_TIMEOUT;
}

static int ata_wait_drq(ata_device *d) {
    uint32_t spins = ATA_TIMEOUT;
    while (spins--) {
        uint8_t st = inb(d->io_base + ATA_STATUS);
        if (st & ATA_SR_ERR) return ATA_E_ERR;
        if (st & ATA_SR_DF)  return ATA_E_FAULT;
        if (!(st & ATA_SR_BSY) && (st & ATA_SR_DRQ)) return 0;
    }
    return ATA_E_TIMEOUT;
}

int ata_identify(ata_device *d) {
    uint16_t id[256];
    int i, rc;
    uint8_t st;

    outb(d->io_base + ATA_DRIVE, (uint8_t)(0xA0 | (d->slave << 4)));
    ata_delay400(d);

    outb(d->io_base + ATA_SECCOUNT, 0);
    outb(d->io_base + ATA_LBA_LO,   0);
    outb(d->io_base + ATA_LBA_MID,  0);
    outb(d->io_base + ATA_LBA_HI,   0);
    outb(d->io_base + ATA_COMMAND,  CMD_IDENTIFY);
    ata_delay400(d);

    st = inb(d->io_base + ATA_STATUS);
    if (st == 0) return ATA_E_NO_DEVICE;     /* floating bus: nothing there */

    rc = ata_wait_not_busy(d);
    if (rc) return rc;

    /* Non-zero LBA_MID/HI here means an ATAPI or SATA device answering
     * the IDENTIFY as something other than a plain ATA disk. */
    if (inb(d->io_base + ATA_LBA_MID) || inb(d->io_base + ATA_LBA_HI))
        return ATA_E_NOT_ATA;

    rc = ata_wait_drq(d);
    if (rc) return rc;

    for (i = 0; i < 256; i++) id[i] = inw(d->io_base + ATA_DATA);

    d->lba48     = (id[83] & (1 << 10)) ? 1 : 0;
    d->sectors   = ((uint32_t)id[61] << 16) | id[60];
    if (d->lba48) {
        uint64_t s = (uint64_t)id[100] | ((uint64_t)id[101] << 16)
                   | ((uint64_t)id[102] << 32) | ((uint64_t)id[103] << 48);
        if (s) d->sectors64 = s; else d->sectors64 = d->sectors;
    } else {
        d->sectors64 = d->sectors;
    }
    d->present = 1;
    return ATA_OK;
}

int ata_init(ata_device *d, uint16_t io_base, uint16_t ctrl_base, int slave) {
    d->io_base   = io_base;
    d->ctrl_base = ctrl_base;
    d->slave     = slave ? 1 : 0;
    d->present   = 0;
    d->lba48     = 0;
    d->sectors   = 0;
    d->sectors64 = 0;
    return ata_identify(d);
}

int ata_read_sectors(ata_device *d, uint64_t lba, uint32_t count, void *buf) {
    uint16_t *dst = (uint16_t *)buf;

    if (!d->present) return ATA_E_NO_DEVICE;
    if (count == 0)  return ATA_OK;
    if (lba + count > d->sectors64) return ATA_E_RANGE;

    while (count > 0) {
        /* One command can transfer at most 256 sectors (LBA28) because the
         * sector-count register is 8 bits and 0 means 256. */
        uint32_t chunk = count > 256 ? 256 : count;
        uint32_t s;
        int rc;

        rc = ata_wait_not_busy(d);
        if (rc) return rc;

        if (d->lba48 && (lba + chunk) > 0x0FFFFFFFull) {
            outb(d->io_base + ATA_DRIVE,
                 (uint8_t)(0x40 | (d->slave << 4)));
            ata_delay400(d);
            /* LBA48 writes each register twice: high byte first. */
            outb(d->io_base + ATA_SECCOUNT, (uint8_t)((chunk >> 8) & 0xFF));
            outb(d->io_base + ATA_LBA_LO,   (uint8_t)((lba >> 24) & 0xFF));
            outb(d->io_base + ATA_LBA_MID,  (uint8_t)((lba >> 32) & 0xFF));
            outb(d->io_base + ATA_LBA_HI,   (uint8_t)((lba >> 40) & 0xFF));
            outb(d->io_base + ATA_SECCOUNT, (uint8_t)(chunk & 0xFF));
            outb(d->io_base + ATA_LBA_LO,   (uint8_t)(lba & 0xFF));
            outb(d->io_base + ATA_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
            outb(d->io_base + ATA_LBA_HI,   (uint8_t)((lba >> 16) & 0xFF));
            outb(d->io_base + ATA_COMMAND,  CMD_READ_PIO48);
        } else {
            if (lba + chunk > 0x10000000ull) return ATA_E_RANGE;
            outb(d->io_base + ATA_DRIVE,
                 (uint8_t)(0xE0 | (d->slave << 4) |
                           (uint8_t)((lba >> 24) & 0x0F)));
            ata_delay400(d);
            outb(d->io_base + ATA_SECCOUNT,
                 (uint8_t)(chunk == 256 ? 0 : chunk));
            outb(d->io_base + ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
            outb(d->io_base + ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
            outb(d->io_base + ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
            outb(d->io_base + ATA_COMMAND, CMD_READ_PIO);
        }

        /* DRQ is raised once per sector, not once per command. */
        for (s = 0; s < chunk; s++) {
            int i;
            rc = ata_wait_drq(d);
            if (rc) return rc;
            for (i = 0; i < 256; i++) *dst++ = inw(d->io_base + ATA_DATA);
        }

        lba   += chunk;
        count -= chunk;
    }
    return ATA_OK;
}

/* josh_blockdev adapter, so fat32.c can sit straight on top. */
static int ata_bdev_read(struct josh_blockdev *dev, uint64_t lba,
                         uint32_t count, void *buf) {
    return ata_read_sectors((ata_device *)dev->ctx, lba, count, buf);
}

void ata_as_blockdev(ata_device *d, josh_blockdev *out) {
    out->read        = ata_bdev_read;
    out->ctx         = d;
    out->sector_size = 512;
}

const char *ata_strerror(int e) {
    switch (e) {
    case ATA_OK:           return "ok";
    case ATA_E_TIMEOUT:    return "drive timeout";
    case ATA_E_ERR:        return "drive reported ERR";
    case ATA_E_FAULT:      return "drive fault";
    case ATA_E_NO_DEVICE:  return "no device";
    case ATA_E_NOT_ATA:    return "not a plain ATA disk";
    case ATA_E_RANGE:      return "LBA out of range";
    }
    return "unknown ATA error";
}
