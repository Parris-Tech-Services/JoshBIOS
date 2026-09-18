#ifndef JOSHBIOS_BOOTINFO_H
#define JOSHBIOS_BOOTINFO_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define JOSH_BOOT_INFO_MAGIC 0x48534f4au
#define JOSH_BOOT_INFO_VERSION 1u
#define JOSH_FIRMWARE_LEGACY_BIOS 1u

typedef struct __attribute__((packed)) {
    u32 magic;
    u16 version;
    u16 size;
    u8 boot_drive;
    u8 firmware_type;
    u16 flags;
    u32 reserved;
} JoshBootInfo;

#endif
