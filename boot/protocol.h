#ifndef JOSHBOOT_PROTOCOL_H
#define JOSHBOOT_PROTOCOL_H

typedef unsigned char jb_u8;
typedef unsigned short jb_u16;
typedef unsigned int jb_u32;
typedef unsigned long long jb_u64;

#define JOSH_BOOT_INFO_MAGIC 0x30494248534f4a4aULL
#define JOSH_BOOT_ABI_MAJOR 0u
#define JOSH_BOOT_ABI_MINOR 1u

#define JOSH_LOADER_MAGIC1 0x544f4f4248534f4aULL
#define JOSH_LOADER_MAGIC2 0x0030564f544f5250ULL

#define JOSH_BOOT_FLAG_MEMORY_MAP (1u << 0)
#define JOSH_BOOT_FLAG_FRAMEBUFFER (1u << 1)
#define JOSH_BOOT_FLAG_RSDP        (1u << 2)
#define JOSH_BOOT_FLAG_SMBIOS      (1u << 3)
#define JOSH_BOOT_FLAG_CMDLINE     (1u << 4)
#define JOSH_BOOT_FLAG_LOADER_NAME (1u << 5)

#define JOSH_FIRMWARE_LEGACY_BIOS 1u
#define JOSH_FIRMWARE_UEFI        2u

#define JOSH_MEMORY_USABLE           1u
#define JOSH_MEMORY_RESERVED         2u
#define JOSH_MEMORY_ACPI_RECLAIMABLE 3u
#define JOSH_MEMORY_ACPI_NVS         4u
#define JOSH_MEMORY_BAD              5u

#define JOSH_BOOT_MAX_MEMORY_ENTRIES 128u

typedef struct __attribute__((packed)) {
    jb_u64 base;
    jb_u64 length;
    jb_u32 type;
    jb_u32 flags;
} JoshMemoryMapEntry;

typedef struct __attribute__((packed)) {
    jb_u64 address;
    jb_u32 width;
    jb_u32 height;
    jb_u32 pitch;
    jb_u32 bpp;
    jb_u32 red_mask_size;
    jb_u32 red_mask_shift;
    jb_u32 green_mask_size;
    jb_u32 green_mask_shift;
    jb_u32 blue_mask_size;
    jb_u32 blue_mask_shift;
} JoshFramebufferInfo;

typedef struct __attribute__((packed)) {
    jb_u64 magic;
    jb_u32 abi_major;
    jb_u32 abi_minor;
    jb_u32 total_size;
    jb_u32 flags;
    jb_u32 firmware_type;
    jb_u32 boot_drive;
    jb_u64 memory_map_address;
    jb_u32 memory_map_entries;
    jb_u32 memory_map_entry_size;
    JoshFramebufferInfo framebuffer;
    jb_u64 kernel_phys_start;
    jb_u64 kernel_phys_end;
    jb_u64 kernel_virt_start;
    jb_u64 kernel_virt_end;
    jb_u64 rsdp_phys;
    jb_u64 smbios_phys;
    jb_u64 command_line_address;
    jb_u32 command_line_length;
    jb_u32 reserved0;
    jb_u64 bootloader_name_address;
    jb_u32 bootloader_name_length;
    jb_u32 reserved1;
    jb_u64 kernel_entry;
    jb_u64 reserved[5];
} JoshBootInfo;

_Static_assert(sizeof(JoshMemoryMapEntry) == 24, "Josh memory map ABI changed");
_Static_assert(sizeof(JoshFramebufferInfo) == 48, "Josh framebuffer ABI changed");
_Static_assert(sizeof(JoshBootInfo) == 224, "Josh boot info ABI changed");

#endif
