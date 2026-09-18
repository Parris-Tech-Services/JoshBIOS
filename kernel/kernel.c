#include "bootinfo.h"
#include "serial.h"

static volatile u16 *const VGA = (volatile u16 *)0xB8000;
static u32 row = 0;
static u32 col = 0;

static void clear_screen(void) {
    for (u32 i = 0; i < 80u * 25u; ++i) {
        VGA[i] = (u16)(' ' | (0x0Fu << 8));
    }
}

static void putc(char c, u8 colour) {
    if (c == '\n') {
        row++;
        col = 0;
        return;
    }
    if (col >= 80) {
        row++;
        col = 0;
    }
    if (row >= 25) {
        row = 0;
    }
    VGA[row * 80 + col] = (u16)((u8)c | ((u16)colour << 8));
    col++;
}

static void print(const char *s, u8 colour) {
    while (*s) {
        putc(*s++, colour);
    }
}

static int valid_boot_info(const JoshBootInfo *info) {
    return info
        && info->magic == JOSH_BOOT_INFO_MAGIC
        && info->version == JOSH_BOOT_INFO_VERSION
        && info->size >= (u16)sizeof(JoshBootInfo);
}

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void kmain(const JoshBootInfo *boot_info) {
    serial_init();
    serial_write("JOSHBIOS_KERNEL_ENTERED\n");

    clear_screen();
    print("JoshBIOS\n", 0x0B);
    print("========\n\n", 0x08);

    if (!valid_boot_info(boot_info)) {
        print("Boot contract validation failed.\n", 0x0C);
        serial_write("JOSHBIOS_ERROR_BOOTINFO\n");
        halt_forever();
    }

    print("Kernel entered successfully.\n", 0x0F);
    print("JoshBootInfo ABI v1 validated.\n", 0x0A);
    print("Stage 1 -> Boot Manager -> protected mode -> kernel\n\n", 0x07);

    if (boot_info->firmware_type == JOSH_FIRMWARE_LEGACY_BIOS) {
        print("Firmware path: legacy BIOS\n", 0x07);
    }
    print("Welcome to the beginning of the Josh boot stack.\n", 0x0A);

    serial_write("JOSHBIOS_BOOTINFO_OK\n");
    serial_write("JOSHBIOS_BOOT_OK\n");
    halt_forever();
}
