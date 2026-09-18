# JoshBIOS architecture

## Current boot path

```text
Power on
  |
  v
Platform BIOS / QEMU SeaBIOS
  |
  v
JoshBIOS Stage 1 (boot sector @ 0x7C00)
  |
  | loads 16 sectors
  v
JoshBIOS Stage 2 (@ 0x8000)
  |
  | BIOS extended disk read
  | enables A20
  | installs GDT
  | enters 32-bit protected mode
  v
JoshBIOS kernel (@ 0x10000)
  |
  v
Direct VGA text output + halt loop
```

The current implementation intentionally targets a simple, observable boot chain before adding x86_64 long mode, interrupts, paging, filesystems and drivers.

## Why the motherboard firmware is not replaced yet

There is no one generic BIOS binary that can safely initialise every PC motherboard. The firmware that runs immediately after reset must understand that board's chipset, memory topology and devices. JoshBIOS will approach this through coreboot on supported hardware rather than claiming a universal flashable ROM.

## ABI direction

A future `JoshBootInfo` structure will carry at least:

- memory map
- framebuffer details
- boot device
- ACPI/SMBIOS pointers
- firmware identity/version
- command line
- loaded module list

The ABI will be versioned before external programs depend on it.
