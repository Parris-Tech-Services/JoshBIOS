# JoshFirmware / JoshBIOS roadmap

This document covers the lowest Josh-owned layers: the path from CPU/platform reset through hardware initialisation, firmware setup and the point where JoshBootloader takes over.

The long-term objective is not a mythical “one BIOS for every PC”. Firmware support must be explicit, board-specific and recoverable.

The first selected physical development system is **DadLAN Laptop #10, a Compaq 610**. Its machine-specific inventory, safety gates and validation checklist live in [the Compaq 610 hardware target document](HARDWARE_TARGET_COMPAQ_610.md). Selection does not mean support: its current status is **Selected**, not yet physically validated.

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

- [x] choose one exact development system — DadLAN Laptop #10 / Compaq 610;
- [ ] **TASK-00868** capture exact baseboard/revision and firmware identifiers;
- [ ] **TASK-00869** identify firmware flash chip;
- [ ] **TASK-00870** obtain/read a known-good firmware image;
- [ ] **TASK-00871** verify external programmer compatibility;
- [ ] **TASK-00872** document recovery wiring and voltage;
- [ ] **TASK-00873** preserve vendor-specific board data where required;
- [ ] **TASK-00874** test recovery procedure before flashing experimental firmware;
- [ ] **TASK-00875** keep experimental and release keys separate.

**Rule:** no “try it and hope” flashing.

A board is not a JoshFirmware target until recovery is understood.

---

# F1 — Emulator firmware path

Use QEMU as the first integration target.

Goals:

- [ ] **TASK-00876** establish reproducible firmware build container/toolchain;
- [ ] **TASK-00877** boot a JoshFirmware/JoshBIOS payload under a supported virtual firmware environment;
- [ ] **TASK-00878** serial console from earliest practical stage;
- [ ] **TASK-00879** deterministic build metadata;
- [ ] **TASK-00880** CI launches firmware → JoshBootloader → test payload;
- [ ] **TASK-00881** record firmware stage timings.

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

- [ ] **TASK-00882** reset-vector path understood;
- [ ] **TASK-00883** CPU microcode policy;
- [ ] **TASK-00884** early temporary execution environment;
- [ ] **TASK-00885** chipset/SoC initialisation;
- [ ] **TASK-00886** clock/timer availability;
- [ ] **TASK-00887** cache configuration required during bring-up;
- [ ] **TASK-00888** CPU feature discovery;
- [ ] **TASK-00889** bootstrap processor/application processor policy;
- [ ] **TASK-00890** platform watchdog handling.

The goal is reliable progression into DRAM-backed execution.

---

# F4 — DRAM

DRAM bring-up is a hard board/silicon-specific boundary.

Roadmap:

- [ ] **TASK-00891** supported DRAM topology documented;
- [ ] **TASK-00892** training path understood;
- [ ] **TASK-00893** failure diagnostics;
- [ ] **TASK-00894** detected capacity passed forward;
- [ ] **TASK-00895** memory test hook for diagnostics;
- [ ] **TASK-00896** cold-boot and warm-reboot repeatability.

Do not hide memory-training failures behind a splash screen. The diagnostic path must still be observable.

---

# F5 — Essential platform devices

Bring up only what is required for firmware and boot:

- [ ] **TASK-00897** SPI/flash access;
- [ ] **TASK-00898** RTC;
- [ ] **TASK-00899** essential PCI/PCIe enumeration;
- [ ] **TASK-00900** boot storage visibility;
- [ ] **TASK-00901** USB keyboard for setup/boot selection where practical;
- [ ] **TASK-00902** display/console path;
- [ ] **TASK-00903** TPM detection;
- [ ] **TASK-00904** basic temperature/fan information if safely accessible;
- [ ] **TASK-00905** firmware settings storage.

General-purpose drivers belong in the OS, not firmware.

---

# F6 — Platform tables

JoshFirmware must eventually provide correct:

- [ ] **TASK-00906** ACPI RSDP/tables;
- [ ] **TASK-00907** MADT/APIC information;
- [ ] **TASK-00908** FADT/power information;
- [ ] **TASK-00909** MCFG where appropriate;
- [ ] **TASK-00910** HPET/table data if used;
- [ ] **TASK-00911** SMBIOS;
- [ ] **TASK-00912** board/vendor identification;
- [ ] **TASK-00913** firmware version/build information.

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

- [ ] **TASK-00914** appears only after display is reliable;
- [ ] **TASK-00915** does not block boot for decorative animation;
- [ ] **TASK-00916** key hints use real supported keys/actions;
- [ ] **TASK-00917** version/build available;
- [ ] **TASK-00918** optional quiet/verbose mode;
- [ ] **TASK-00919** serial logging continues underneath.

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

- [ ] **TASK-00920** versioned settings schema;
- [ ] **TASK-00921** defaults;
- [ ] **TASK-00922** validation;
- [ ] **TASK-00923** recovery from corrupt settings;
- [ ] **TASK-00924** atomic update where possible;
- [ ] **TASK-00925** reset-to-default;
- [ ] **TASK-00926** migration between firmware versions.

Never allow one corrupt setting record to brick boot.

---

# F11 — Firmware update system

Design before first user-friendly updater exists.

Needs:

- [ ] **TASK-00927** signed firmware artefacts;
- [ ] **TASK-00928** board identity check;
- [ ] **TASK-00929** image compatibility check;
- [ ] **TASK-00930** power/interruption strategy;
- [ ] **TASK-00931** previous-known-good or recovery image;
- [ ] **TASK-00932** update status logging;
- [ ] **TASK-00933** explicit development override;
- [ ] **TASK-00934** anti-rollback policy only once recovery is mature.

The firmware updater must refuse an image for the wrong board.

---

# F12 — Security roadmap

Progressively add:

- [ ] **TASK-00935** reproducible builds;
- [ ] **TASK-00936** release signing;
- [ ] **TASK-00937** measured boot;
- [ ] **TASK-00938** TPM event log;
- [ ] **TASK-00939** Secure Boot-compatible UEFI path if adopted;
- [ ] **TASK-00940** firmware write-protection strategy;
- [ ] **TASK-00941** update signature verification;
- [ ] **TASK-00942** development/release trust separation;
- [ ] **TASK-00943** secrets/keys never embedded in public source.

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

The first machine used for this milestone is the **Compaq 610** documented in [HARDWARE_TARGET_COMPAQ_610.md](HARDWARE_TARGET_COMPAQ_610.md). Passing it validates that exact machine only; broader support must be earned target by target.

A legitimate first board milestone requires:

- [ ] **TASK-00944** exact board documented;
- [ ] **TASK-00945** recovery tested;
- [ ] **TASK-00946** repeated cold boot;
- [ ] **TASK-00947** repeated warm reboot;
- [ ] **TASK-00948** correct RAM amount;
- [ ] **TASK-00949** boot storage visible;
- [ ] **TASK-00950** keyboard/setup path;
- [ ] **TASK-00951** display path;
- [ ] **TASK-00952** ACPI/SMBIOS sanity;
- [ ] **TASK-00953** JoshBootloader hand-off;
- [ ] **TASK-00954** canonical Josh kernel boot;
- [ ] **TASK-00955** clean shutdown/reboot from OS;
- [ ] **TASK-00956** known limitations documented.

The bar is “recoverable and repeatable”, not merely “logo appeared once”.
