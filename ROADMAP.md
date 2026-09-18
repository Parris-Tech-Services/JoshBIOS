# JoshBIOS / JoshFirmware / JoshBootloader roadmap

This repository owns the low-level Josh boot research stack. Its small local kernel is a **test payload**. The canonical native Josh OS kernel lives in [joshuaparris-max/AshFallen](https://github.com/joshuaparris-max/AshFallen).

## Detailed roadmaps

- [JoshFirmware / JoshBIOS](docs/FIRMWARE_ROADMAP.md) — reset-to-loader platform bring-up, start screen, setup, security, recovery and physical-board support.
- [JoshBootloader](docs/BOOTLOADER_ROADMAP.md) — filesystems, ELF64, long mode, boot menu, UEFI/BIOS adapters, recovery and verification.
- [Josh Boot Protocol](docs/JOSH_BOOT_PROTOCOL.md) — bootloader-side hand-off contract.
- [Architecture](docs/ARCHITECTURE.md) — current experimental boot chain.

The matching canonical-kernel contract is documented in `AshFallen/docs/BOOT_ABI.md`.

## v0.1 — First light

- [x] own 512-byte BIOS boot sector;
- [x] own Stage 2 loader;
- [x] BIOS EDD/LBA disk loading;
- [x] Stage 2 development boot menu with timeout, diagnostics and reboot;
- [x] versioned JoshBootInfo v1 development hand-off;
- [x] A20 enable;
- [x] GDT and 32-bit protected mode;
- [x] freestanding test payload;
- [x] direct VGA output;
- [x] reproducible raw boot image;
- [x] CI build and structural checks;
- [x] headless QEMU boot smoke test with serial success markers.

## v0.2 — Boot the real Josh kernel

Preserve the tiny payload as a regression target, but focus new boot engineering on the canonical kernel.

- [ ] block/storage abstraction instead of fixed sectors;
- [ ] MBR/GPT discovery;
- [ ] FAT32 reader;
- [ ] ELF64 validation/loading;
- [ ] x86-64 long-mode hand-off;
- [ ] serial diagnostics;
- [ ] Josh Boot Protocol v0 builder;
- [ ] memory-map hand-off;
- [ ] framebuffer hand-off;
- [ ] ACPI/SMBIOS hand-off;
- [ ] load `AshFallen/kernel`;
- [ ] reach the canonical kernel boot-success marker in QEMU;
- [ ] keep Limine as a reference path until parity is proven.

## v0.3 — Real boot manager

- [ ] versioned boot configuration;
- [ ] normally hidden/short-timeout boot menu;
- [ ] Josh OS default entry;
- [ ] previous-known-good entry;
- [ ] recovery entry;
- [ ] diagnostics/development entry;
- [ ] boot-attempt health state;
- [ ] automatic menu after repeated failure;
- [ ] kernel/module hashing;
- [ ] typed error screen instead of silent halt.

The current branded Limine **Josh OS Boot Manager** in AshFallen is the behaviour/reference to converge with, not a second permanent boot-manager architecture.

## v0.4 — UEFI + legacy adapters

- [ ] isolate legacy BIOS services behind an adapter;
- [ ] UEFI application/adapter;
- [ ] GOP framebuffer;
- [ ] UEFI memory map;
- [ ] correct ExitBootServices flow;
- [ ] entropy/RNG input where available;
- [ ] UEFI chainload support only if needed;
- [ ] BIOS and UEFI QEMU matrix.

## v0.5 — JoshBIOS firmware environment

- [ ] firmware start/splash screen;
- [ ] hardware inventory;
- [ ] firmware setup;
- [ ] one-time boot-device menu;
- [ ] boot order;
- [ ] recovery/diagnostics entry;
- [ ] firmware settings storage;
- [ ] platform-table hand-off;
- [ ] measured/verified boot plumbing;
- [ ] firmware update/rollback design.

## v0.6 — JoshFirmware platform integration

- [ ] choose one exact development board/system;
- [ ] document recovery/programmer procedure;
- [ ] reproducible firmware-foundation build;
- [ ] board-specific hardware bring-up;
- [ ] DRAM/platform validation;
- [ ] ACPI/SMBIOS sanity;
- [ ] repeated cold/warm boot;
- [ ] JoshBootloader hand-off;
- [ ] canonical Josh kernel boot;
- [ ] shutdown/reboot validation.

## v1.0 — Recoverable supported boot stack

A v1.0 claim requires more than reaching a logo once:

- [ ] explicit supported-hardware matrix;
- [ ] external recovery path;
- [ ] signed/reproducible release artefacts;
- [ ] previous-known-good rollback;
- [ ] automated QEMU end-to-end tests;
- [ ] firmware → bootloader → canonical kernel integration test;
- [ ] visible diagnostics for every critical failure stage;
- [ ] documented security/update policy;
- [ ] repeatable physical cold boot on at least one exact supported target.

## Engineering rule

Firmware, bootloader and kernel responsibilities stay separate. The local payload proves the loader; it does not compete with the canonical Josh kernel. The stack converges through a versioned boot contract.
