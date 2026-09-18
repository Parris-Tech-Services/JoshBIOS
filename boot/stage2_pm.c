#include "elf64.h"
#include "protocol.h"
#include "core/block.h"
#include "core/partition.h"
#include "core/fat32.h"
#include "core/config.h"
#include "core/health.h"

#define KERNEL_IMAGE_ADDR 0x01000000u
#define KERNEL_IMAGE_BYTES (4u * 1024u * 1024u)
#define BIOS_BOUNCE_ADDR 0x00060000u
#define BIOS_BOUNCE_SECTORS 64u
#define CONFIG_PATH "/boot/josh/boot.cfg"
#define BIOS_DEVICE_SECTORS 0x100000000ULL

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
#define CMDLINE_ADDR 0x0005c000u
#define HEALTH_BUFFER_ADDR 0x0005d000u

#define PAGE_PRESENT 0x001ULL
#define PAGE_RW 0x002ULL
#define PAGE_PS 0x080ULL

#define COM1 0x3f8u

extern unsigned short e820_count;
extern unsigned char boot_drive;
extern unsigned char filesystem_boot;
extern void enter_long_mode(void);
extern int bios_block_read(void *context, unsigned long long lba,
                           unsigned int sector_count, void *buffer);
extern int bios_block_write(void *context, unsigned long long lba,
                            unsigned int sector_count, const void *buffer);

unsigned long long stage2_kernel_entry;
static int health_persistent_enabled;

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

static int cpu_has_cpuid(void) {
    unsigned int original;
    unsigned int changed;

    __asm__ volatile (
        "pushfl\n\t"
        "popl %0\n\t"
        : "=r"(original)
    );

    unsigned int toggled = original ^ (1u << 21);
    __asm__ volatile (
        "pushl %1\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %0\n\t"
        "pushl %2\n\t"
        "popfl\n\t"
        : "=r"(changed)
        : "r"(toggled), "r"(original)
        : "cc"
    );

    return ((changed ^ original) & (1u << 21)) != 0;
}

static void cpuid(
    unsigned int leaf,
    unsigned int *eax_out,
    unsigned int *ebx_out,
    unsigned int *ecx_out,
    unsigned int *edx_out
) {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(0u)
    );
    if (eax_out) *eax_out = eax;
    if (ebx_out) *ebx_out = ebx;
    if (ecx_out) *ecx_out = ecx;
    if (edx_out) *edx_out = edx;
}

static int cpu_has_long_mode(void) {
    if (!cpu_has_cpuid()) return 0;

    unsigned int eax;
    unsigned int edx;
    cpuid(1u, &eax, 0, 0, &edx);
    if ((edx & (1u << 6)) == 0) return 0; /* PAE */

    cpuid(0x80000000u, &eax, 0, 0, 0);
    if (eax < 0x80000001u) return 0;

    cpuid(0x80000001u, 0, 0, 0, &edx);
    return (edx & (1u << 29)) != 0; /* Long mode */
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

static int bios_block_read_bounced(
    void *context,
    unsigned long long lba,
    unsigned int sector_count,
    void *buffer
) {
    (void)context;
    if (!buffer || sector_count == 0u) return -1;

    unsigned char *destination = (unsigned char *)buffer;
    unsigned int remaining = sector_count;
    while (remaining != 0u) {
        unsigned int chunk =
            remaining > BIOS_BOUNCE_SECTORS ?
                BIOS_BOUNCE_SECTORS : remaining;
        if (bios_block_read(
                0,
                lba,
                chunk,
                (void *)BIOS_BOUNCE_ADDR) != 0) {
            return -1;
        }

        unsigned int bytes = chunk * JOSH_BLOCK_SECTOR_SIZE;
        memcopy(destination, (const void *)BIOS_BOUNCE_ADDR, bytes);
        destination += bytes;
        lba += chunk;
        remaining -= chunk;
    }

    return 0;
}

static int staging_range_is_usable(void) {
    const JoshMemoryMapEntry *entries =
        (const JoshMemoryMapEntry *)MEMORY_MAP_ADDR;
    unsigned long long start = KERNEL_IMAGE_ADDR;
    unsigned long long end = start + KERNEL_IMAGE_BYTES;

    for (unsigned int i = 0; i < e820_count; ++i) {
        if (entries[i].type != JOSH_MEMORY_USABLE) continue;
        unsigned long long entry_start = entries[i].base;
        unsigned long long entry_end = entry_start + entries[i].length;
        if (entry_end < entry_start) continue;
        if (start >= entry_start && end <= entry_end) return 1;
    }
    return 0;
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

static unsigned int text_length(const char *text) {
    unsigned int length = 0;
    if (!text) return 0;
    while (text[length]) ++length;
    return length;
}

static unsigned char checksum(
    const unsigned char *memory,
    unsigned int length
) {
    unsigned char sum = 0;
    while (length--) sum = (unsigned char)(sum + *memory++);
    return sum;
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

static int health_read_copy(
    unsigned long long lba,
    josh_boot_health_record_t *record
) {
    if (bios_block_read(0, lba, 1u, (void *)HEALTH_BUFFER_ADDR) != 0) return 0;
    memcopy(record, (const void *)HEALTH_BUFFER_ADDR, sizeof(*record));
    return josh_boot_health_valid(record);
}

static int health_write_copy(
    unsigned long long lba,
    const josh_boot_health_record_t *record
) {
    memzero((void *)HEALTH_BUFFER_ADDR, JOSH_BOOT_HEALTH_SECTOR_SIZE);
    memcopy((void *)HEALTH_BUFFER_ADDR, record, sizeof(*record));
    if (bios_block_write(0, lba, 1u, (const void *)HEALTH_BUFFER_ADDR) != 0) return 0;

    josh_boot_health_record_t verify;
    if (!health_read_copy(lba, &verify)) return 0;
    return verify.generation == record->generation &&
           verify.checksum == record->checksum;
}

static int health_load(josh_boot_health_record_t *record) {
    josh_boot_health_record_t a;
    josh_boot_health_record_t b;
    memzero(&a, sizeof(a));
    memzero(&b, sizeof(b));

    health_persistent_enabled = 0;
    int have_a = health_read_copy(JOSH_BOOT_HEALTH_LBA_A, &a);
    int have_b = health_read_copy(JOSH_BOOT_HEALTH_LBA_B, &b);

    if (!have_a && !have_b) {
        josh_boot_health_default(record);
        serial_write("JOSHBOOT_HEALTH_VOLATILE_UNPROVISIONED\n");
        return 1;
    }

    (void)josh_boot_health_choose(&a, &b, record);
    health_persistent_enabled = 1;
    serial_write("JOSHBOOT_HEALTH_LOADED\n");
    return 1;
}

static int health_persist(const josh_boot_health_record_t *record) {
    if (!health_persistent_enabled) {
        serial_write("JOSHBOOT_HEALTH_VOLATILE_NO_WRITE\n");
        return 0;
    }

    unsigned long long lba =
        (record->generation & 1u) ?
            JOSH_BOOT_HEALTH_LBA_B : JOSH_BOOT_HEALTH_LBA_A;
    if (!health_write_copy(lba, record)) {
        health_persistent_enabled = 0;
        serial_write("JOSHBOOT_WARN_HEALTH_PERSIST\n");
        return 0;
    }
    serial_write("JOSHBOOT_HEALTH_PERSISTED\n");
    return 1;
}

static const char *kernel_path_for_slot(
    const josh_boot_config_t *config,
    josh_boot_slot_t slot
) {
    if (slot == JOSH_BOOT_SLOT_CURRENT) return config->kernel_path;
    if (slot == JOSH_BOOT_SLOT_PREVIOUS) return config->previous_kernel_path;
    return config->recovery_kernel_path;
}

static void write_slot_marker(josh_boot_slot_t slot) {
    if (slot == JOSH_BOOT_SLOT_CURRENT) {
        serial_write("JOSHBOOT_SLOT_CURRENT\n");
    } else if (slot == JOSH_BOOT_SLOT_PREVIOUS) {
        serial_write("JOSHBOOT_SLOT_PREVIOUS\n");
    } else {
        serial_write("JOSHBOOT_SLOT_RECOVERY\n");
    }
}

static int read_kernel_candidate(
    josh_fat32_t *filesystem,
    const char *path,
    unsigned int *image_bytes
) {
    if (!path || path[0] == '\0') return -1;

    josh_fat32_file_t kernel;
    if (josh_fat32_open_path(filesystem, path, &kernel) != JOSH_FAT32_OK) {
        return -1;
    }
    if (kernel.size == 0 || kernel.size > KERNEL_IMAGE_BYTES) return -1;

    unsigned int bytes_read = 0;
    if (josh_fat32_read_file(
            filesystem,
            &kernel,
            0,
            (void *)KERNEL_IMAGE_ADDR,
            kernel.size,
            &bytes_read
        ) != JOSH_FAT32_OK ||
        bytes_read != kernel.size) {
        return -1;
    }

    JoshElf64Summary summary;
    if (josh_elf64_validate(
            (const void *)KERNEL_IMAGE_ADDR,
            kernel.size,
            &summary
        ) != JOSH_ELF64_OK) {
        return -1;
    }

    *image_bytes = kernel.size;
    return 0;
}

static int load_kernel_file(unsigned int *image_bytes, josh_boot_config_t *config) {
    if (!image_bytes || !config) return -1;

    josh_block_device_t device;
    device.context = 0;
    device.sector_count = BIOS_DEVICE_SECTORS;
    device.read = bios_block_read_bounced;

    josh_partition_t partition;
    if (josh_partition_find_boot(&device, &partition) != JOSH_PARTITION_OK) {
        serial_write("JOSHBOOT_ERROR_PARTITION\n");
        return -1;
    }
    serial_write("JOSHBOOT_PARTITION_OK\n");

    int health_layout_safe = josh_partition_range_is_unallocated(
        &device, JOSH_BOOT_HEALTH_LBA_A, 2u);
    if (!health_layout_safe) {
        serial_write("JOSHBOOT_HEALTH_LAYOUT_UNSAFE_VOLATILE\n");
    }

    josh_fat32_t filesystem;
    if (josh_fat32_mount(&device, &partition, &filesystem) != JOSH_FAT32_OK) {
        serial_write("JOSHBOOT_ERROR_FAT32\n");
        return -1;
    }
    serial_write("JOSHBOOT_FAT32_OK\n");

    josh_fat32_file_t config_file;
    if (josh_fat32_open_path(&filesystem, CONFIG_PATH, &config_file) != JOSH_FAT32_OK) {
        serial_write("JOSHBOOT_ERROR_CONFIG_PATH\n");
        return -1;
    }
    if (config_file.size == 0 || config_file.size > JOSH_BOOT_CONFIG_MAX_BYTES) {
        serial_write("JOSHBOOT_ERROR_CONFIG_SIZE\n");
        return -1;
    }

    char config_buffer[JOSH_BOOT_CONFIG_MAX_BYTES];
    unsigned int config_bytes = 0;
    if (josh_fat32_read_file(
            &filesystem,
            &config_file,
            0,
            config_buffer,
            config_file.size,
            &config_bytes
        ) != JOSH_FAT32_OK ||
        config_bytes != config_file.size) {
        serial_write("JOSHBOOT_ERROR_CONFIG_READ\n");
        return -1;
    }

    if (josh_boot_config_parse(config_buffer, config_bytes, config) != JOSH_CONFIG_OK) {
        serial_write("JOSHBOOT_ERROR_CONFIG_PARSE\n");
        return -1;
    }
    serial_write("JOSHBOOT_CONFIG_OK\n");

    josh_boot_health_record_t health;
    if (!health_layout_safe) {
        josh_boot_health_default(&health);
        health_persistent_enabled = 0;
        serial_write("JOSHBOOT_HEALTH_VOLATILE_LAYOUT\n");
    } else if (!health_load(&health)) {
        serial_write("JOSHBOOT_ERROR_HEALTH_LOAD\n");
        return -1;
    }

    uint32_t old_slot = health.selected_slot;
    uint64_t old_generation = health.generation;
    int was_pending = health.pending_good != 0u;
    josh_boot_slot_t slot = josh_boot_health_prepare_attempt(
        &health, JOSH_BOOT_HEALTH_MAX_ATTEMPTS);

    if (health.generation != old_generation) (void)health_persist(&health);
    if (was_pending && old_slot != (uint32_t)slot) {
        if (slot == JOSH_BOOT_SLOT_PREVIOUS) {
            serial_write("JOSHBOOT_HEALTH_ROLLBACK_PREVIOUS\n");
        } else {
            serial_write("JOSHBOOT_HEALTH_ROLLBACK_RECOVERY\n");
        }
    }

    for (unsigned int candidate = 0; candidate < 3u; ++candidate) {
        const char *path = kernel_path_for_slot(config, slot);
        write_slot_marker(slot);

        if (read_kernel_candidate(&filesystem, path, image_bytes) == 0) {
            serial_write("JOSHBOOT_KERNEL_PATH_OK\n");
            serial_write("JOSHBOOT_KERNEL_FILE_LOADED\n");
            return 0;
        }

        if (slot == JOSH_BOOT_SLOT_CURRENT &&
            config->previous_kernel_path[0] != '\0') {
            if (!josh_boot_health_select_fallback(
                    &health,
                    JOSH_BOOT_SLOT_PREVIOUS,
                    JOSH_BOOT_FAILURE_KERNEL_FILE)) {
                break;
            }
            (void)health_persist(&health);
            serial_write("JOSHBOOT_FALLBACK_PREVIOUS\n");
            slot = JOSH_BOOT_SLOT_PREVIOUS;
            continue;
        }

        if (slot != JOSH_BOOT_SLOT_RECOVERY &&
            config->recovery_kernel_path[0] != '\0') {
            if (!josh_boot_health_select_fallback(
                    &health,
                    JOSH_BOOT_SLOT_RECOVERY,
                    JOSH_BOOT_FAILURE_KERNEL_FILE)) {
                break;
            }
            (void)health_persist(&health);
            serial_write("JOSHBOOT_FALLBACK_RECOVERY\n");
            slot = JOSH_BOOT_SLOT_RECOVERY;
            continue;
        }

        break;
    }

    serial_write("JOSHBOOT_ERROR_KERNEL_CANDIDATES\n");
    return -1;
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

static int load_elf(JoshElf64Summary *summary, unsigned int image_bytes) {
    const void *image = (const void *)KERNEL_IMAGE_ADDR;
    JoshElf64Status status =
        josh_elf64_validate(image, image_bytes, summary);

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
                image_bytes,
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

static void fill_boot_info(const JoshElf64Summary *summary, const josh_boot_config_t *config) {
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

    if (config && config->command_line[0] != '\0') {
        unsigned int command_line_length = text_length(config->command_line);
        memcopy((void *)CMDLINE_ADDR, config->command_line, command_line_length);
        info->command_line_address = CMDLINE_ADDR;
        info->command_line_length = command_line_length;
        info->flags |= JOSH_BOOT_FLAG_CMDLINE;
    }

    const char name[] = "JoshBootloader BIOS";
    memcopy((void *)LOADER_NAME_ADDR, name, sizeof(name) - 1);
    info->bootloader_name_address = LOADER_NAME_ADDR;
    info->bootloader_name_length = sizeof(name) - 1;
}

int stage2_pm_main(void) {
    unsigned char *image = (unsigned char *)KERNEL_IMAGE_ADDR;
    unsigned int image_bytes = KERNEL_IMAGE_BYTES;
    josh_boot_config_t boot_config;
    josh_boot_config_t *active_config = 0;

    serial_init();

    if (!cpu_has_long_mode()) {
        serial_write("JOSHBOOT_ERROR_CPU_LONG_MODE\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    serial_write("JOSHBOOT_CPU_LONG_MODE_OK\n");

    if (filesystem_boot && !staging_range_is_usable()) {
        serial_write("JOSHBOOT_ERROR_STAGING_MEMORY\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    if (image[0] != 0x7f || image[1] != 'E' ||
        image[2] != 'L' || image[3] != 'F') {
        if (!filesystem_boot) {
            return 0;
        }

        serial_write("JOSHBOOT_FS_LOAD_BEGIN\n");
        if (load_kernel_file(&image_bytes, &boot_config) != 0) {
            serial_write("JOSHBOOT_ERROR_FILESYSTEM_LOAD\n");
            for (;;) __asm__ volatile ("cli; hlt");
        }

        active_config = &boot_config;

        if (image[0] != 0x7f || image[1] != 'E' ||
            image[2] != 'L' || image[3] != 'F') {
            serial_write("JOSHBOOT_ERROR_KERNEL_NOT_ELF\n");
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }

    serial_write("JOSHBOOT_ELF64_DETECTED\n");

    JoshElf64Summary summary;
    if (load_elf(&summary, image_bytes) != 0) {
        serial_write("JOSHBOOT_ERROR_ELF64\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    fill_boot_info(&summary, active_config);
    build_page_tables();
    stage2_kernel_entry = summary.entry;

    serial_write("JOSHBOOT_HANDOFF_READY\n");
    enter_long_mode();

    for (;;) __asm__ volatile ("cli; hlt");
}
