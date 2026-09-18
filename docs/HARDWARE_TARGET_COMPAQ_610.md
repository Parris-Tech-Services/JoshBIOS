# First physical hardware target — Compaq 610

## Status

**Selected development target — not yet physically validated and not yet a supported Josh OS/JoshFirmware machine.**

DadLAN Laptop #10, a **Compaq 610**, is the first physical machine the Josh OS stack will deliberately target. The purpose is to give firmware, bootloader and kernel work one concrete hardware baseline before expanding to additional machines.

A successful boot on this laptop proves only that this exact target reached the tested milestone. It does not imply generic PC, Compaq-family or Core 2-era hardware support.

## Recorded inventory

The project currently records the following inventory for DadLAN Laptop #10:

| Component | Recorded value | Verification status |
| --- | --- | --- |
| System | Compaq 610 | Known from fleet inventory |
| CPU | Intel Core 2 Duo T5870 | Recorded; x86-64 capable |
| RAM | 3 GB | Recorded |
| Internal storage | 120 GB HDD | Recorded; intentionally not SSD-upgraded |
| Graphics | Intel GMA X3100 | Recorded |
| Current installed OS | Windows 10 32-bit | Recorded |

These values are enough to select the machine as a boot-test target. They are **not** enough to flash replacement firmware. Before firmware work depends on a field, re-read it from the physical machine and record the exact identifier/revision here.

## Why this machine

The Compaq 610 is useful as the first target because it is:

- an expendable DadLAN development machine rather than a primary computer;
- x86-64 capable, so it can run the canonical Josh kernel;
- old enough to exercise the legacy-PC path that JoshBootloader already researches;
- equipped with an internal keyboard that is likely to expose the early PS/2 path;
- simple enough to make initial hardware bring-up observable.

Those are development advantages, not claims of current compatibility.

## Phase A — non-destructive Josh OS boot testing

Keep the internal HDD intact initially.

1. Build or download the current canonical native Josh OS ISO from AshFallen.
2. Boot it from removable media/USB, using Limine as the reference path.
3. Record whether the firmware can select and start the media.
4. Record every visible Josh boot stage and any failure screen.
5. Verify framebuffer output.
6. Verify the detected memory map/RAM amount.
7. Verify built-in keyboard input.
8. Exercise repeated cold boots and warm reboots.
9. Boot the Stage 0 product ISO separately where useful for Linux-backed hardware inventory and desktop validation.
10. Do not install to or repartition the internal HDD until removable-media boot is repeatable.

The native kernel currently lacks many normal laptop facilities, including mature storage, USB, networking, audio, accelerated graphics and power-management support. Missing functionality on this target should become explicit driver or subsystem work rather than being hidden behind a generic “hardware support” claim.

## Phase B — exact machine inventory

Before making the kernel or bootloader machine-specific, capture the exact hardware identifiers:

- BIOS vendor, version and date;
- whether the machine exposes legacy BIOS only or any EFI capability;
- SMBIOS system and baseboard manufacturer/product/revision;
- CPU family/model/stepping and relevant CPUID features;
- northbridge/southbridge/chipset IDs;
- RAM topology and, where practical, SPD information;
- Intel graphics PCI ID and usable firmware framebuffer/VBE modes;
- SATA/IDE storage controller PCI ID and mode;
- Ethernet controller PCI ID;
- Wi-Fi controller PCI ID, if fitted;
- audio controller/codec;
- USB controller types and PCI IDs;
- PS/2/i8042 behaviour for keyboard and touchpad;
- ACPI RSDP/table set;
- EC/super-I/O identity where discoverable;
- SPI flash manufacturer, exact part number, capacity and voltage;
- firmware write-protection state;
- any board-specific data that must survive a firmware rewrite.

Prefer a Linux live environment for inventory collection where it gives better visibility. Save raw command output or dumps alongside a short human-readable summary when useful.

## Phase C — JoshBootloader + canonical kernel

The highest-value integration goal remains:

```text
Compaq 610 power-on firmware
        ↓
JoshBootloader
        ↓
Josh Boot Protocol
        ↓
AshFallen x86-64 Josh kernel
        ↓
observable JOSHOS_BOOT_OK / graphical boot evidence
```

Until that works, Limine remains the reference boot path. Do not fork the canonical kernel for the Compaq 610; add boot adapters and hardware drivers to the one Josh kernel.

Target-specific work should still use clean subsystem boundaries. A Compaq-specific quirk belongs behind the relevant platform/driver interface, not scattered through generic kernel code.

## Firmware research baseline

The Compaq 610 family has multiple system-board variants, so the exact board in DadLAN Laptop #10 must still be read from the physical machine before flashing. However, the recorded T5870 + Intel integrated-graphics configuration lines up strongly with HP's **GM UMA** board class.

Current research baseline:

| Item | Research result | Confidence |
| --- | --- | --- |
| HP system-board spare | **538409-001** for Intel + GM/UMA configurations | High for this hardware class; verify label |
| Platform family | Inventec **Vulcain / VV09** | High for the Intel UMA schematic family |
| Candidate board ID | **VV09-6050A2256501-MB-A04** appears in a matching T5870/Compaq 610 repair dump; schematic is AX1/A01 family | Candidate only — physically verify |
| Northbridge | Intel **GME965** for the GM UMA configuration | High for T5870/800 MHz FSB UMA model |
| Southbridge | Intel **ICH8M** | High |
| BIOS flash device in Vulcain UMA schematic | **SST25VF080B**, reference **U23**, SOIC-8, 8 Mbit / 1 MiB SPI NOR | High for schematic family; physically verify marking |
| Flash operating voltage | **2.7–3.6 V**, nominally 3.3 V | Datasheet-backed for SST25VF080B |
| BIOS rail in schematic | **+V3A** | Schematic-backed |
| Flashrom support | SST25VF080B is listed as tested for probe/read/erase/write | Upstream flashrom source |
| Likely Intel BIOS family | **68PVU** for Intel-video Compaq 610 variants | Strong secondary-source evidence; verify current BIOS ID |
| Known later vendor BIOS | **F.20**, released 2 Dec 2011 | Secondary archive; verify against HP package for exact product number |

The schematic identifies the SPI chip as U23, but this is **not permission to connect a programmer blindly**. Before attaching power or a clip, read the marking on Laptop #10's actual U23 and confirm the board ID/revision.

### External programmer plan

Prefer an **external 3.3 V-capable SPI programmer** with a SOIC-8 test clip or, if in-circuit reads are unstable, remove the chip and use a socket/adapter.

The chosen programmer must:

- drive the SST25VF080B within its 2.7–3.6 V range;
- support standard SPI read/erase/write/verify;
- not place 5 V logic on the flash pins;
- allow repeatable full-chip reads before any erase/write operation.

Do not use flashrom's forced internal-laptop flashing path as the recovery strategy. Upstream flashrom explicitly warns that laptop ECs can interfere with internal flash access.

### Backup acceptance test

Before any experimental write:

1. Disconnect AC power and main battery before attaching an external programmer unless the exact in-circuit power method has been deliberately validated.
2. Identify pin 1 and confirm clip orientation.
3. Probe the chip and confirm it identifies as the physically observed part.
4. Read the complete chip at least **three times**.
5. Hash all dumps with SHA-256; all three must be byte-identical.
6. Store the original dump in at least two separate locations, with one copy marked read-only.
7. Record board ID, chip marking, programmer model, programmer voltage, flashrom/programmer version and hashes.
8. Inspect the image for plausible firmware structure and preserve machine-specific data such as serial/MAC/platform data.
9. Perform a verify-only read comparison before considering a write.

If repeated reads differ, **stop**. That means the clip, power arrangement or in-circuit bus isolation is not reliable enough for a safe write.

### Recovery procedure — not yet tested

The intended hard recovery path is external reprogramming of U23 from the verified original 1 MiB dump. This is not considered tested until Laptop #10 has been deliberately recovered from a controlled non-booting test image or equivalent safe fixture.

There is historical evidence of HP Win+B/HP_TOOLS recovery on this product generation, but that is only a secondary recovery path. It must not replace the external programmer because a sufficiently broken boot block may prevent firmware-assisted recovery from starting.

## Phase D — JoshFirmware safety gate

**Do not flash JoshFirmware/JoshBIOS replacement firmware to this laptop yet.**

Before the first experimental firmware write, all of the following are required:

- [ ] exact baseboard/revision identified;
- [ ] exact SPI flash chip identified;
- [ ] flash voltage known;
- [ ] compatible external programmer and clip/adapter available;
- [ ] known-good full firmware image read from the laptop;
- [ ] repeated reads compare identically;
- [ ] board-specific data locations understood and preserved;
- [ ] external recovery procedure documented;
- [ ] recovery procedure tested using a known-good image;
- [ ] coreboot support/porting status for the exact board understood;
- [ ] experimental image has an explicit board identity check.

Internal software flashing is not the recovery plan. The development machine does not become a JoshFirmware target until an external recovery path is proven.

## Evidence ladder for this machine

Use these labels literally:

- **Selected** — chosen as the development target.
- **Inventoried** — exact relevant hardware/firmware identifiers captured.
- **Boot-tested** — a particular image was deliberately exercised and the observed result recorded.
- **Integrated** — the relevant Josh layers work together on this machine.
- **Supported** — the documented configuration works repeatedly with a documented recovery path and known limitations.

Current status: **Selected**.

## First physical target exit criteria

Before calling the Compaq 610 a supported Josh OS development machine:

- [ ] exact machine inventory committed;
- [ ] native Josh OS boots from removable media repeatedly;
- [ ] graphical output is stable;
- [ ] correct RAM/memory-map behaviour observed;
- [ ] built-in keyboard path works;
- [ ] failures produce understandable diagnostics rather than a permanent blank screen;
- [ ] repeated cold boots pass;
- [ ] repeated warm reboots pass;
- [ ] JoshBootloader can load the canonical AshFallen kernel;
- [ ] Josh Boot Protocol hand-off is validated on the machine;
- [ ] shutdown/reboot behaviour is documented;
- [ ] supported and unsupported devices are listed;
- [ ] firmware recovery is proven before any JoshFirmware flash is attempted.

## Expansion policy

Only after the Compaq 610 baseline is repeatable should the project add another physical target.

The next target should preferably exercise meaningfully different hardware — for example a UEFI-era x86-64 system — so that generic abstractions are tested rather than merely copying Compaq-specific assumptions.

Every additional machine gets its own descriptor, test evidence, limitations and recovery notes. One working laptop never becomes evidence for “generic PC support”.
