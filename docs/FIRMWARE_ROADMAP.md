# JoshFirmware / JoshBIOS roadmap

This document covers the lowest Josh-owned layers: the path from CPU/platform reset through hardware initialisation, firmware setup and the point where JoshBootloader takes over.

The long-term objective is not a mythical “one BIOS for every PC”. Firmware support must be explicit, board-specific and recoverable.

## Responsibility split

### JoshFirmware

Owns or coordinates:

- board-specific platform bring-up;
- CPU/chipset/SoC initialisation;
- DRAM initialisation/training;
- essential buses and boot-device visibility;
- ACPI/SMBIOS platform information;
- firmware variables/settings storage;
- firmware security measurements and update plumbing.

Initially this should build on a proven firmware foundation such as coreboot for specifically supported boards rather than reimplementing silicon bring-up blindly.

### JoshBIOS

Owns the Josh-facing firmware environment:

- startup/splash screen;
- firmware setup;
- boot order and one-time boot policy;
- hardware inventory;
- diagnostics/recovery entry;
- hand-off into JoshBootloader.

The name “JoshBIOS” is a product/firmware-environment name. The implementation may use modern firmware mechanisms and should not imply that the system is limited to legacy BIOS interfaces.

---

# F0 — Development safety first

Before physical firmware experiments:

- [ ] choose one exact development motherboard/system;
- [ ] identify firmware flash chip;
- [ ] obtain/read a known-good firmware image;
- [ ] verify external programmer compatibility;
- [ ] document recovery wiring and voltage;
- [ ] preserve vendor-specific board data where required;
- [ ] test recovery procedure before flashing experimental firmware;
- [ ] keep experimental and release keys separate.

**Rule:** no “try it and hope” flashing.

A board is not a JoshFirmware target until recovery is understood.

---

# F1 — Emulator firmware path

Use QEMU as the first integration target.

Goals:

- [ ] establish reproducible firmware build container/toolchain;
- [ ] boot a JoshFirmware/JoshBIOS payload under a supported virtual firmware environment;
- [ ] serial console from earliest practical stage;
- [ ] deterministic build metadata;
- [ ] CI launches firmware → JoshBootloader → test payload;
- [ ] record firmware stage timings.

This gives us a place to develop UX and hand-off contracts without risking hardware.

---

# F2 — Board support definition

For every physical target, create a machine descriptor documenting:

- manufacturer/model/revision;
- CPU/SoC;
- chipset/PCH;
- DRAM topology;
- SPI flash part and size;
- EC/super-I/O;
- storage controllers;
- network devices;
- display/graphics path;
- USB controllers;
- TPM;
- firmware recovery method;
- known binary blobs;
- unsupported features.

Never claim generic support based on one machine in the same product family.

---

# F3 — CPU and silicon bring-up

Depending on platform/foundation:

- [ ] reset-vector path understood;
- [ ] CPU microcode policy;
- [ ] early temporary execution environment;
- [ ] chipset/SoC initialisation;
- [ ] clock/timer availability;
- [ ] cache configuration required during bring-up;
- [ ] CPU feature discovery;
- [ ] bootstrap processor/application processor policy;
- [ ] platform watchdog handling.

The goal is reliable progression into DRAM-backed execution.

---

# F4 — DRAM

DRAM bring-up is a hard board/silicon-specific boundary.

Roadmap:

- [ ] supported DRAM topology documented;
- [ ] training path understood;
- [ ] failure diagnostics;
- [ ] detected capacity passed forward;
- [ ] memory test hook for diagnostics;
- [ ] cold-boot and warm-reboot repeatability.

Do not hide memory-training failures behind a splash screen. The diagnostic path must still be observable.

---

# F5 — Essential platform devices

Bring up only what is required for firmware and boot:

- [ ] SPI/flash access;
- [ ] RTC;
- [ ] essential PCI/PCIe enumeration;
- [ ] boot storage visibility;
- [ ] USB keyboard for setup/boot selection where practical;
- [ ] display/console path;
- [ ] TPM detection;
- [ ] basic temperature/fan information if safely accessible;
- [ ] firmware settings storage.

General-purpose drivers belong in the OS, not firmware.

---

# F6 — Platform tables

JoshFirmware must eventually provide correct:

- [ ] ACPI RSDP/tables;
- [ ] MADT/APIC information;
- [ ] FADT/power information;
- [ ] MCFG where appropriate;
- [ ] HPET/table data if used;
- [ ] SMBIOS;
- [ ] board/vendor identification;
- [ ] firmware version/build information.

Incorrect platform tables are worse than missing optional features because they can destabilise the kernel in subtle ways.

---

# F7 — JoshBIOS start screen

Yes, the stack should have a real **firmware start screen**.

## Normal mode

Keep it calm and brief:

```text
            JOSH
        Firmware x.y

       Starting Josh OS…

Setup   Boot Menu   Recovery
```

Exact visual design is not frozen, but behaviour should be.

Requirements:

- [ ] appears only after display is reliable;
- [ ] does not block boot for decorative animation;
- [ ] key hints use real supported keys/actions;
- [ ] version/build available;
- [ ] optional quiet/verbose mode;
- [ ] serial logging continues underneath.

## Failure mode

If firmware cannot proceed:

- explicit failed stage;
- board ID;
- error code;
- recovery action;
- log/serial availability.

Never leave the user with only a logo.

---

# F8 — Firmware setup

A deliberately small setup UI.

## System

- CPU;
- RAM;
- board;
- firmware build;
- time/date.

## Boot

- default boot entry;
- boot-device order;
- timeout;
- one-time next boot;
- external media policy.

## Security

- TPM state;
- Secure Boot state when relevant;
- firmware verification state;
- development/release mode.

## Devices

Only settings that are truly supported and safe.

## Recovery

- reset firmware settings;
- boot recovery image;
- previous-known-good firmware;
- diagnostics;
- firmware update/recovery.

Avoid exposing obscure silicon switches simply because traditional firmware does.

---

# F9 — One-time boot menu

Firmware boot-device selection is distinct from the JoshBootloader OS/kernel menu.

Firmware menu chooses **where to boot from**, for example:

- internal system disk;
- USB/removable media;
- network boot later if explicitly supported;
- firmware recovery image.

JoshBootloader then chooses **what Josh system/kernel entry to boot** from the selected device.

This separation avoids duplicated policy.

---

# F10 — Firmware settings storage

Requirements:

- [ ] versioned settings schema;
- [ ] defaults;
- [ ] validation;
- [ ] recovery from corrupt settings;
- [ ] atomic update where possible;
- [ ] reset-to-default;
- [ ] migration between firmware versions.

Never allow one corrupt setting record to brick boot.

---

# F11 — Firmware update system

Design before first user-friendly updater exists.

Needs:

- [ ] signed firmware artefacts;
- [ ] board identity check;
- [ ] image compatibility check;
- [ ] power/interruption strategy;
- [ ] previous-known-good or recovery image;
- [ ] update status logging;
- [ ] explicit development override;
- [ ] anti-rollback policy only once recovery is mature.

The firmware updater must refuse an image for the wrong board.

---

# F12 — Security roadmap

Progressively add:

- [ ] reproducible builds;
- [ ] release signing;
- [ ] measured boot;
- [ ] TPM event log;
- [ ] Secure Boot-compatible UEFI path if adopted;
- [ ] firmware write-protection strategy;
- [ ] update signature verification;
- [ ] development/release trust separation;
- [ ] secrets/keys never embedded in public source.

Security state should be passed to the bootloader/kernel as facts through the Josh Boot Protocol.

---

# F13 — Recovery

Firmware recovery should survive broken higher layers.

Possible recovery inputs:

- external programmer for development boards;
- protected recovery region where hardware permits;
- removable recovery media;
- previous-known-good image;
- minimal firmware recovery UI.

A recovery path must not depend on the normal OS being bootable.

---

# F14 — Performance

Measure, do not guess.

Track:

- reset → first serial marker;
- reset → DRAM ready;
- DRAM ready → display;
- display → firmware ready;
- firmware ready → JoshBootloader;
- total firmware duration.

Optimise expensive stages without removing diagnostics.

---

# F15 — Firmware CI

Virtual CI should verify:

- reproducible build;
- image size/layout constraints;
- firmware boots in QEMU;
- serial milestones appear;
- bootloader is found;
- kernel hand-off occurs;
- failure/recovery fixtures behave as expected.

Physical firmware CI can later use a sacrificial test machine with remotely controlled power and serial capture, but only after the virtual path is mature.

---

# First physical-hardware milestone

A legitimate first board milestone requires:

- [ ] exact board documented;
- [ ] recovery tested;
- [ ] repeated cold boot;
- [ ] repeated warm reboot;
- [ ] correct RAM amount;
- [ ] boot storage visible;
- [ ] keyboard/setup path;
- [ ] display path;
- [ ] ACPI/SMBIOS sanity;
- [ ] JoshBootloader hand-off;
- [ ] canonical Josh kernel boot;
- [ ] clean shutdown/reboot from OS;
- [ ] known limitations documented.

The bar is “recoverable and repeatable”, not merely “logo appeared once”.
