#include "elf64.h"
#include "protocol.h"
#include "core/block.h"
#include "core/partition.h"
#include "core/fat32.h"

#define KERNEL_IMAGE_ADDR 0x00010000u
#define KERNEL_IMAGE_BYTES (512u * 512u)
#define KERNEL_PATH "/BOOT/JOSH/KERNEL.ELF"

#define KERNEL_VIRT_BASE 0xffffffff80000000ULL
#define KERNEL_PHYS_BASE 0x00200000u
#define KERNEL_PHYS_LIMIT 0x08000000u

#define PML4_ADDR 0x00050000u
#define PDPT_LOW_ADDR 0x00051000u
#define PD_LOW0_ADDR 0x00052000u
#define PDPT_HIGH_ADDR 0x00056000u
#define PD_HIGH_ADDR 0x00057000u

#define BOOTINFO_ADDR 0x00058000u
#define MEMORY_MAP_ADDR 0x00059000u
#define VBE_INFO_ADDR 0x0005a000u
#define LOADER_NAME_ADDR 0x0005b000u

#define PAGE_PRESENT 0x001ULL
#define PAGE_RW 0x002ULL
#define PAGE_PS 0x080ULL

#define COM1 0x3f8u

extern unsigned short e820_count;
extern unsigned char vbe_ready;
extern unsigned char boot_drive;
extern unsigned long long bios_disk_sectors;
extern int bios_block_read(void *context, unsigned long long lba,
                           unsigned int sector_count, void *buffer);
extern void enter_long_mode(void);

unsigned long long stage2_kernel_entry;

static inline void out8(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0,%1" : : "a"(value), "Nd"(port));
}

static inline unsigned char in8(unsigned short port) {
    unsigned char value;
    __asm__ volatile ("inb %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void) {
    out8(COM1 + 1u, 0);
    out8(COM1 + 3u, 0x80);
    out8(COM1, 1);
    out8(COM1 + 1u, 0);
    out8(COM1 + 3u, 3);
    out8(COM1 + 2u, 0xc7);
    out8(COM1 + 4u, 0x0b);
}

static void serial_write(const char *message) {
    while (*message) {
        while ((in8(COM1 + 5u) & 0x20u) == 0) {
        }
        out8(COM1, (unsigned char)*message++);
    }
}

static void memzero(void *destination, unsigned int length) {
    unsigned char *d = destination;
    while (length--) *d++ = 0;
}

static void memcopy(void *destination, const void *source, unsigned int length) {
    unsigned char *d = destination;
    const unsigned char *s = source;
    while (length--) *d++ = *s++;
}

static int bytes_equal(
    const unsigned char *memory,
    const char *text,
    unsigned int length
) {
    while (length--) {
        if (*memory++ != (unsigned char)*text++) return 0;
    }
    return 1;
}

static unsigned char checksum(
    const unsigned char *memory,
    unsigned int length
) {
    unsigned char sum = 0;
    while (length--) sum = (unsigned char)(sum + *memory++);
    return sum;
}

static int load_kernel_from_filesystem(void) {
    if (bios_disk_sectors < 2u) {
        serial_write("JOSHBOOT_FAT32_NO_DISK_BOUNDS\n");
        return -1;
    }

    josh_block_device_t device = {
        .context = 0,
        .sector_count = bios_disk_sectors,
        .read = bios_block_read
    };

    josh_partition_t partition;
    if (josh_partition_find_boot(&device, &partition) != JOSH_PARTITION_OK) {
        serial_write("JOSHBOOT_FAT32_PARTITION_UNAVAILABLE\n");
        return -1;
    }

    josh_fat32_t filesystem;
    if (josh_fat32_mount(&device, &partition, &filesystem) != JOSH_FAT32_OK) {
        serial_write("JOSHBOOT_FAT32_MOUNT_FAILED\n");
        return -1;
    }

    josh_fat32_file_t kernel;
    if (josh_fat32_open_path(&filesystem, KERNEL_PATH, &kernel) != JOSH_FAT32_OK) {
        serial_write("JOSHBOOT_FAT32_KERNEL_NOT_FOUND\n");
        return -1;
    }

    if (kernel.size == 0 || kernel.size > KERNEL_IMAGE_BYTES) {
        serial_write("JOSHBOOT_FAT32_KERNEL_SIZE_INVALID\n");
        return -1;
    }

    memzero((void *)KERNEL_IMAGE_ADDR, KERNEL_IMAGE_BYTES);

    unsigned int bytes_read = 0;
    if (josh_fat32_read_file(
            &filesystem,
            &kernel,
            0,
            (void *)KERNEL_IMAGE_ADDR,
            kernel.size,
            &bytes_read
        ) != JOSH_FAT32_OK || bytes_read != kernel.size) {
        serial_write("JOSHBOOT_FAT32_KERNEL_READ_FAILED\n");
        return -1;
    }

    serial_write("JOSHBOOT_FAT32_KERNEL_OK\n");
    return 0;
}

static unsigned long long find_rsdp(void) {
    unsigned int ebda_segment;
    __asm__ volatile ("movzwl 0x40e, %0" : "=r"(ebda_segment));
    unsigned int ebda = ebda_segment << 4;

    unsigned int starts[2] = {ebda, 0xe0000u};
    unsigned int ends[2] = {ebda + 1024u, 0x100000u};

    for (unsigned int region = 0; region < 2; ++region) {
        for (unsigned int address = (starts[region] + 15u) & ~15u;
             address + 20u <= ends[region];
             address += 16u) {
            unsigned char *candidate = (unsigned char *)address;
            if (!bytes_equal(candidate, "RSD PTR ", 8)) continue;
            if (checksum(candidate, 20) != 0) continue;

            unsigned char revision = candidate[15];
            if (revision >= 2) {
                unsigned int length = *(unsigned int *)(candidate + 20);
                if (length < 36u || address + length > ends[region] ||
                    checksum(candidate, length) != 0) {
                    continue;
                }
            }
            return address;
        }
    }
    return 0;
}

static unsigned long long find_smbios(void) {
    for (unsigned int address = 0xf0000u;
         address + 0x20u <= 0x100000u;
         address += 16u) {
        unsigned char *candidate = (unsigned char *)address;

        if (bytes_equal(candidate, "_SM3_", 5)) {
            unsigned char length = candidate[6];
            if (length >= 0x18u && checksum(candidate, length) == 0) {
                return address;
            }
        }

        if (bytes_equal(candidate, "_SM_", 4)) {
            unsigned char length = candidate[5];
            if (length >= 0x1fu && checksum(candidate, length) == 0) {
                return address;
            }
        }
    }
    return 0;
}

static void build_page_tables(void) {
    memzero((void *)PML4_ADDR, 0x8000u);

    volatile unsigned long long *pml4 =
        (volatile unsigned long long *)PML4_ADDR;
    volatile unsigned long long *pdpt =
        (volatile unsigned long long *)PDPT_LOW_ADDR;

    pml4[0] = PDPT_LOW_ADDR | PAGE_PRESENT | PAGE_RW;

    for (unsigned int gib = 0; gib < 4; ++gib) {
        unsigned int pd_address = PD_LOW0_ADDR + gib * 0x1000u;
        pdpt[gib] = pd_address | PAGE_PRESENT | PAGE_RW;

        volatile unsigned long long *pd =
            (volatile unsigned long long *)pd_address;

        for (unsigned int i = 0; i < 512; ++i) {
            unsigned long long physical =
                ((unsigned long long)gib * 512ULL + i) * 0x200000ULL;
            pd[i] = physical | PAGE_PRESENT | PAGE_RW | PAGE_PS;
        }
    }

    volatile unsigned long long *pdpt_high =
        (volatile unsigned long long *)PDPT_HIGH_ADDR;
    volatile unsigned long long *pd_high =
        (volatile unsigned long long *)PD_HIGH_ADDR;

    pml4[511] = PDPT_HIGH_ADDR | PAGE_PRESENT | PAGE_RW;
    pdpt_high[510] = PD_HIGH_ADDR | PAGE_PRESENT | PAGE_RW;

    for (unsigned int i = 0; i < 512; ++i) {
        pd_high[i] =
            (KERNEL_PHYS_BASE + (unsigned long long)i * 0x200000ULL) |
            PAGE_PRESENT | PAGE_RW | PAGE_PS;
    }
}

static int load_elf(JoshElf64Summary *summary) {
    const void *image = (const void *)KERNEL_IMAGE_ADDR;
    JoshElf64Status status =
        josh_elf64_validate(image, KERNEL_IMAGE_BYTES, summary);

    if (status != JOSH_ELF64_OK) return -1;
    if (summary->virtual_min < KERNEL_VIRT_BASE ||
        summary->virtual_max <= summary->virtual_min) {
        return -1;
    }

    if (summary->virtual_max - KERNEL_VIRT_BASE >
        (unsigned long long)(KERNEL_PHYS_LIMIT - KERNEL_PHYS_BASE)) {
        return -1;
    }

    for (unsigned short i = 0; i < summary->load_segment_count; ++i) {
        JoshElf64LoadSegment segment;
        if (josh_elf64_load_segment(
                image,
                KERNEL_IMAGE_BYTES,
                i,
                &segment
            ) != JOSH_ELF64_OK) {
            return -1;
        }

        if (segment.virtual_address < KERNEL_VIRT_BASE) return -1;

        unsigned long long offset =
            segment.virtual_address - KERNEL_VIRT_BASE;
        unsigned long long physical = KERNEL_PHYS_BASE + offset;

        if (physical > KERNEL_PHYS_LIMIT ||
            segment.memory_size >
                (unsigned long long)KERNEL_PHYS_LIMIT - physical) {
            return -1;
        }

        memcopy(
            (void *)(unsigned int)physical,
            (const unsigned char *)image + (unsigned int)segment.file_offset,
            (unsigned int)segment.file_size
        );

        if (segment.memory_size > segment.file_size) {
            memzero(
                (void *)(unsigned int)(physical + segment.file_size),
                (unsigned int)(segment.memory_size - segment.file_size)
            );
        }
    }

    return 0;
}

static void fill_boot_info(const JoshElf64Summary *summary) {
    JoshBootInfo *info = (JoshBootInfo *)BOOTINFO_ADDR;
    memzero(info, sizeof(*info));

    info->magic = JOSH_BOOT_INFO_MAGIC;
    info->abi_major = JOSH_BOOT_ABI_MAJOR;
    info->abi_minor = JOSH_BOOT_ABI_MINOR;
    info->total_size = sizeof(*info);
    info->flags =
        JOSH_BOOT_FLAG_MEMORY_MAP |
        JOSH_BOOT_FLAG_FRAMEBUFFER |
        JOSH_BOOT_FLAG_LOADER_NAME;
    info->firmware_type = JOSH_FIRMWARE_LEGACY_BIOS;
    info->boot_drive = boot_drive;

    info->memory_map_address = MEMORY_MAP_ADDR;
    info->memory_map_entries = e820_count;
    info->memory_map_entry_size = sizeof(JoshMemoryMapEntry);

    volatile unsigned char *vbe =
        (volatile unsigned char *)VBE_INFO_ADDR;

    info->framebuffer.pitch =
        *(volatile unsigned short *)(vbe + 0x10);
    info->framebuffer.width =
        *(volatile unsigned short *)(vbe + 0x12);
    info->framebuffer.height =
        *(volatile unsigned short *)(vbe + 0x14);
    info->framebuffer.bpp = vbe[0x19];
    info->framebuffer.red_mask_size = vbe[0x1f];
    info->framebuffer.red_mask_shift = vbe[0x20];
    info->framebuffer.green_mask_size = vbe[0x21];
    info->framebuffer.green_mask_shift = vbe[0x22];
    info->framebuffer.blue_mask_size = vbe[0x23];
    info->framebuffer.blue_mask_shift = vbe[0x24];
    info->framebuffer.address =
        *(volatile unsigned int *)(vbe + 0x28);

    info->kernel_virt_start = summary->virtual_min;
    info->kernel_virt_end = summary->virtual_max;
    info->kernel_phys_start =
        KERNEL_PHYS_BASE + (summary->virtual_min - KERNEL_VIRT_BASE);
    info->kernel_phys_end =
        KERNEL_PHYS_BASE + (summary->virtual_max - KERNEL_VIRT_BASE);
    info->kernel_entry = summary->entry;

    info->rsdp_phys = find_rsdp();
    if (info->rsdp_phys) info->flags |= JOSH_BOOT_FLAG_RSDP;

    info->smbios_phys = find_smbios();
    if (info->smbios_phys) info->flags |= JOSH_BOOT_FLAG_SMBIOS;

    const char name[] = "JoshBootloader BIOS";
    memcopy((void *)LOADER_NAME_ADDR, name, sizeof(name) - 1);
    info->bootloader_name_address = LOADER_NAME_ADDR;
    info->bootloader_name_length = sizeof(name) - 1;
}

int stage2_pm_main(void) {
    serial_init();

    /*
     * Filesystem loading is the normal path. Failure is deliberately nonfatal
     * while the verified raw bootstrap remains our recovery/development path.
     */
    (void)load_kernel_from_filesystem();

    unsigned char *image = (unsigned char *)KERNEL_IMAGE_ADDR;
    if (image[0] != 0x7f || image[1] != 'E' ||
        image[2] != 'L' || image[3] != 'F') {
        serial_write("JOSHBOOT_NO_ELF64_USING_LEGACY_FALLBACK\n");
        return 0;
    }

    serial_write("JOSHBOOT_ELF64_DETECTED\n");

    if (e820_count == 0) {
        serial_write("JOSHBOOT_ERROR_E820\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    if (!vbe_ready) {
        serial_write("JOSHBOOT_ERROR_VBE\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    JoshElf64Summary summary;
    if (load_elf(&summary) != 0) {
        serial_write("JOSHBOOT_ERROR_ELF64\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    fill_boot_info(&summary);
    build_page_tables();
    stage2_kernel_entry = summary.entry;

    serial_write("JOSHBOOT_HANDOFF_READY\n");
    enter_long_mode();

    for (;;) __asm__ volatile ("cli; hlt");
}
