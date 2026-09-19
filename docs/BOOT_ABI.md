# Josh boot ABI

The boundary between firmware/bootloader and kernel is a product interface, not an implementation detail.

## v1 development ABI

The current legacy-BIOS development path passes a pointer in **EAX** to a packed `JoshBootInfo` structure located at physical address `0x7000`.

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;         // "JOSH" / 0x48534f4a
    uint16_t version;       // 1
    uint16_t size;          // sizeof(JoshBootInfo)
    uint8_t  boot_drive;    // BIOS DL value
    uint8_t  firmware_type; // 1 = legacy BIOS
    uint16_t flags;
    uint32_t reserved;
} JoshBootInfo;
```

This is deliberately small. It proves that the bootloader and kernel communicate through a versioned contract rather than hidden assumptions.

## Planned ABI growth

Before the Josh boot stack attempts to boot the canonical x86-64 Josh OS kernel in `joshuaparris-max/JoshOS`, the ABI should grow to describe:

- usable/reserved memory map;
- framebuffer address, geometry, pitch and pixel masks;
- ACPI RSDP and SMBIOS pointers;
- boot device identity;
- firmware identity/version;
- kernel command line;
- loaded modules/initramfs;
- boot reason/recovery mode;
- ABI feature flags.

The x86-64 hand-off should use the normal 64-bit calling convention and a stable, documented pointer rather than keeping the current temporary 32-bit EAX convention.

## Compatibility rule

A kernel must validate `magic`, `version` and `size` before consuming any fields. New fields are appended; existing fields do not change meaning inside a major ABI version.
