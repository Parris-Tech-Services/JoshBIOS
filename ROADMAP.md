# JoshBIOS / JoshFirmware / JoshBootloader roadmap

This repository owns the low-level Josh boot research stack. Its small local kernel is a **test payload**. The canonical native Josh OS kernel lives in [joshuaparris-max/JoshOS](https://github.com/joshuaparris-max/JoshOS).

## Detailed roadmaps

- [JoshFirmware / JoshBIOS](docs/FIRMWARE_ROADMAP.md) — reset-to-loader platform bring-up, start screen, setup, security, recovery and physical-board support.
- [JoshBootloader](docs/BOOTLOADER_ROADMAP.md) — filesystems, ELF64, long mode, boot menu, UEFI/BIOS adapters, recovery and verification.
- [Josh Boot Protocol](docs/JOSH_BOOT_PROTOCOL.md) — bootloader-side hand-off contract.
- [Architecture](docs/ARCHITECTURE.md) — current experimental boot chain.

The matching canonical-kernel contract is documented in `JoshOS/docs/BOOT_ABI.md`.

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

- [x] block/storage abstraction on the legacy-BIOS path;
- [x] MBR/GPT discovery (host-tested; MBR integrated in the current boot image);
- [x] FAT32 reader + 8.3 path lookup;
- [x] load `/boot/josh/kernel.elf` from FAT32 rather than a fixed kernel extent;
- [x] ELF64 validation/loading;
- [x] x86-64 long-mode hand-off;
- [x] serial diagnostics;
- [x] Josh Boot Protocol v0 builder;
- [x] memory-map hand-off;
- [x] framebuffer hand-off;
- [x] explicitly assert ACPI/SMBIOS hand-off in integration CI;
- [x] load the canonical `JoshOS/kernel`;
- [x] reach the canonical kernel boot-success marker in QEMU;
- [x] keep Limine working as the reference path.

GitHub Actions run `35345509864` additionally proves that the legacy-BIOS bridge reports `JOSHBOOT_CPU_LONG_MODE_OK`, delivers non-zero ACPI RSDP and SMBIOS pointers to JoshOS (`JOSHOS_RSDP_OK` / `JOSHOS_SMBIOS_OK`), and still reaches `JOSHOS_BOOT_OK`.

## v0.3 — Real boot manager

- [ ] **TASK-00607** versioned boot configuration;
- [ ] **TASK-00608** normally hidden/short-timeout boot menu;
- [ ] **TASK-00609** Josh OS default entry;
- [ ] **TASK-00610** previous-known-good entry;
- [ ] **TASK-00611** recovery entry;
- [ ] **TASK-00612** diagnostics/development entry;
- [ ] **TASK-00613** boot-attempt health state;
- [ ] **TASK-00614** automatic menu after repeated failure;
- [ ] **TASK-00615** kernel/module hashing;
- [ ] **TASK-00616** typed error screen instead of silent halt.

The current branded Limine **Josh OS Boot Manager** in JoshOS is the behaviour/reference to converge with, not a second permanent boot-manager architecture.

## v0.4 — UEFI + legacy adapters

- [ ] **TASK-00617** isolate legacy BIOS services behind an adapter;
- [ ] **TASK-00618** UEFI application/adapter;
- [ ] **TASK-00619** GOP framebuffer;
- [ ] **TASK-00620** UEFI memory map;
- [ ] **TASK-00621** correct ExitBootServices flow;
- [ ] **TASK-00622** entropy/RNG input where available;
- [ ] **TASK-00623** UEFI chainload support only if needed;
- [ ] **TASK-00624** BIOS and UEFI QEMU matrix.

## v0.5 — JoshBIOS firmware environment

- [ ] **TASK-00625** firmware start/splash screen;
- [ ] **TASK-00626** hardware inventory;
- [ ] **TASK-00627** firmware setup;
- [ ] **TASK-00628** one-time boot-device menu;
- [ ] **TASK-00629** boot order;
- [ ] **TASK-00630** recovery/diagnostics entry;
- [ ] **TASK-00631** firmware settings storage;
- [ ] **TASK-00632** platform-table hand-off;
- [ ] **TASK-00633** measured/verified boot plumbing;
- [ ] **TASK-00634** firmware update/rollback design.

## v0.6 — JoshFirmware platform integration

- [ ] **TASK-00635** choose one exact development board/system;
- [ ] **TASK-00636** document recovery/programmer procedure;
- [ ] **TASK-00637** reproducible firmware-foundation build;
- [ ] **TASK-00638** board-specific hardware bring-up;
- [ ] **TASK-00639** DRAM/platform validation;
- [ ] **TASK-00640** ACPI/SMBIOS sanity;
- [ ] **TASK-00641** repeated cold/warm boot;
- [ ] **TASK-00642** JoshBootloader hand-off;
- [ ] **TASK-00643** canonical Josh kernel boot;
- [ ] **TASK-00644** shutdown/reboot validation.

## v1.0 — Recoverable supported boot stack

A v1.0 claim requires more than reaching a logo once:

- [ ] **TASK-00645** explicit supported-hardware matrix;
- [ ] **TASK-00646** external recovery path;
- [ ] **TASK-00647** signed/reproducible release artefacts;
- [ ] **TASK-00648** previous-known-good rollback;
- [ ] **TASK-00649** automated QEMU end-to-end tests;
- [ ] **TASK-00650** firmware → bootloader → canonical kernel integration test;
- [ ] **TASK-00651** visible diagnostics for every critical failure stage;
- [ ] **TASK-00652** documented security/update policy;
- [ ] **TASK-00653** repeatable physical cold boot on at least one exact supported target.

## Engineering rule

Firmware, bootloader and kernel responsibilities stay separate. The local payload proves the loader; it does not compete with the canonical Josh kernel. The stack converges through a versioned boot contract.
