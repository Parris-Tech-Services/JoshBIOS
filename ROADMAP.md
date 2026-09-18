# JoshBIOS roadmap

## v0.1 — First light

- [x] Own 512-byte BIOS boot sector
- [x] Own Stage 2 loader
- [x] BIOS LBA disk loading
- [x] A20 enable
- [x] GDT and 32-bit protected mode
- [x] Freestanding C kernel
- [x] Direct VGA text output
- [x] Reproducible raw boot image
- [x] CI build and structural checks

## v0.2 — A real little kernel

- [ ] x86_64 long mode
- [ ] Paging and physical memory manager
- [ ] IDT, exceptions and IRQ handling
- [ ] PIT/APIC timer
- [ ] PS/2 keyboard input
- [ ] Serial debug console
- [ ] Kernel panic screen
- [ ] Versioned JoshBootInfo hand-off

## v0.3 — Bootloader grows up

- [ ] Read FAT32 rather than fixed sectors
- [ ] Load ELF64 kernels by program headers
- [ ] Boot menu and timeout
- [ ] Kernel command line
- [ ] Memory-map discovery
- [ ] UEFI boot path alongside legacy BIOS
- [ ] Hybrid USB/ISO release image

## v0.4 — JoshBIOS firmware environment

- [ ] coreboot payload prototype
- [ ] Hardware inventory screen
- [ ] Boot-order UI
- [ ] Recovery boot entry
- [ ] Firmware settings storage
- [ ] ACPI/SMBIOS hand-off

## v1.0 — Supported physical hardware

- [ ] Select one documented development motherboard
- [ ] Reproducible coreboot-based firmware build
- [ ] External-programmer recovery procedure
- [ ] Secure update/rollback design
- [ ] Hardware compatibility matrix
- [ ] Automated QEMU integration tests
