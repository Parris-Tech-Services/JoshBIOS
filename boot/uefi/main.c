#include "efi.h"

#define COM1_BASE 0x3F8u

static inline void io_out8(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t io_in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void) {
    io_out8(COM1_BASE + 1u, 0x00u);
    io_out8(COM1_BASE + 3u, 0x80u);
    io_out8(COM1_BASE + 0u, 0x01u);
    io_out8(COM1_BASE + 1u, 0x00u);
    io_out8(COM1_BASE + 3u, 0x03u);
    io_out8(COM1_BASE + 2u, 0xC7u);
    io_out8(COM1_BASE + 4u, 0x0Bu);
}

static void serial_write(const char *message) {
    while (*message != '\0') {
        while ((io_in8(COM1_BASE + 5u) & 0x20u) == 0u) {
        }
        io_out8(COM1_BASE, (uint8_t)*message);
        ++message;
    }
}

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    (void)image_handle;
    (void)system_table;

    serial_init();
    serial_write("JOSHUEFI_ENTRY_OK\r\n");

    return EFI_SUCCESS;
}
