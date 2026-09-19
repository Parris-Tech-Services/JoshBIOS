# JoshBIOS architecture

## Current boot paths

The tiny local 32-bit payload remains an unpartitioned regression image, but the verified canonical-kernel path is:

```text
Power on
  |
  v
Platform BIOS / QEMU SeaBIOS
  |
  v
JoshBIOS Stage 1 (boot sector @ 0x7C00)
  |
  | BIOS EDD loads Stage 2
  v
JoshBootloader Stage 2 (@ 0x8000)
  |
  | MBR partition discovery
  | FAT32 mount
  | open /BOOT/JOSH/KERNEL.ELF
  | ELF64 validate/load
  | E820 + VBE discovery
  | ACPI RSDP / SMBIOS scan
  | construct Josh Boot Protocol
  | build bootstrap page tables
  | enter x86-64 long mode
  v
canonical JoshOS kernel
  |
  v
JOSHOS_BOOT_OK
```

GitHub Actions verifies this legacy-BIOS path in QEMU. GPT parsing exists and is host-tested but the integration image currently uses MBR. The UEFI path is still only a boot-tested entry scaffold and does not yet load the canonical kernel.

## Why the motherboard firmware is not replaced yet

There is no one generic BIOS binary that can safely initialise every PC motherboard. The firmware that runs immediately after reset must understand that board's chipset, memory topology and devices. JoshBIOS will approach this through coreboot on supported hardware rather than claiming a universal flashable ROM.

## Boot ABI

The experimental versioned `JoshBootInfo` structure now carries:

- memory map
- framebuffer details
- boot device
- ACPI/SMBIOS pointers
- firmware identity/version
- command line
- loaded module list

The ABI will be versioned before external programs depend on it.
