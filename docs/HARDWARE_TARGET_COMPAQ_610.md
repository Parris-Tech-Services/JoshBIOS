# First physical hardware target — Compaq 610

## Status

**Selected development target — not yet physically validated and not yet a supported Josh OS/JoshFirmware machine.**

DadLAN Laptop #10, a **Compaq 610**, is the first physical machine the Josh OS stack will deliberately target. The purpose is to give firmware, bootloader and kernel work one concrete hardware baseline before expanding to additional machines.

A successful boot on this laptop proves only that this exact target reached the tested milestone. It does not imply generic PC, Compaq-family or Core 2-era hardware support.

## Current inventory: exact unit vs model-family facts

### Confirmed/recorded for DadLAN Laptop #10

These are the strongest current unit-specific records from the DadLAN inventory and prior hardware captures:

| Component | Current record | Status |
| --- | --- | --- |
| Fleet identity | DadLAN Laptop #10 | Confirmed |
| System | HP Compaq 610 | Confirmed |
| Product/SKU | VE908PA#ABG | Recorded from prior inventory; re-read service tag before firmware work |
| Hostname | DESKTOP-MMR0H5N | Confirmed from Windows/Action1 inventory |
| CPU | Intel Core 2 Duo T5870 @ 2.00 GHz, 2 cores / 2 threads | Confirmed |
| CPU ISA | x86-64 / Intel 64 capable; Execute Disable support | Model-backed and consistent with installed CPU |
| RAM | 3 GB DDR2 | Latest observed inventory; supersedes older 2 GB / 4 GB notes |
| Graphics | Mobile Intel 965 Express / GMA X3100 integrated graphics | Confirmed |
| Internal storage | Hitachi HTS543212L9A300, 120 GB class (111.8 GiB usable), 2.5-inch SATA HDD | Confirmed |
| HDD class | Travelstar 5K320, 5400 rpm, SATA 3 Gb/s, non-Advanced-Format | Model-backed |
| Current OS | Windows 10 Home 32-bit | Confirmed |
| Firmware mode | Legacy-PC generation; current exact BIOS version and boot-mode settings not yet captured | Partially known |
| Josh OS status | Selected first physical Josh OS target | Confirmed project status |
| Josh OS physical boot | No successful physical Josh OS boot yet recorded | Not yet tested |

Older notes that described Laptop #10 as 2 GB or 4 GB RAM were provisional/earlier observations. Until a fresh hardware inventory says otherwise, **3 GB is the canonical current value**.

### Original VE908PA configuration evidence

Period Australian retailer listings for **VE908PA** consistently identify a Compaq 610 configuration with:

- Core 2 Duo T5870;
- 1 GB DDR2 originally;
- 160 GB HDD originally;
- 15.6-inch display;
- DVD-RW;
- integrated Intel graphics;
- Windows XP Professional / Vista Business-era licensing;
- some bundles advertised a 2 MP webcam and RAM upgrade.

This is useful historical context only. Laptop #10 has clearly changed since new because its current recorded storage is a 120 GB Hitachi drive and current RAM is 3 GB.

### Model-family hardware strongly indicated by the installed T5870/X3100 configuration

| Subsystem | Model-family result | Exact-unit status |
| --- | --- | --- |
| Northbridge | Intel GME965 for T5870 + integrated graphics configuration | Strongly indicated; verify PCI ID |
| Southbridge | Intel ICH8M | Strongly indicated; verify PCI ID |
| System board spare | HP 538409-001 GM/UMA board is the expected board class | Candidate; verify board label/revision |
| Wired NIC | Marvell integrated 10/100 Ethernet; QuickSpecs driver name 88E8042 | Family-level; verify exact PCI ID |
| Audio | IDT 92HD75 High Definition Audio family | Family-level; verify codec ID |
| Wi-Fi | Could be Intel 802.11a/b/g, Broadcom 4312G b/g, or another approved option | Unknown on this exact unit |
| Bluetooth | Optional factory module | Unknown on this exact unit |
| Webcam | Optional 2 MP module | Unknown on this exact unit |
| Display | 15.6-inch 1366×768; LED/CCFL, anti-glare/BrightView variants existed | Resolution/fitted panel still needs direct verification |
| Optical drive | DVD-ROM or DVD±RW variants existed | Exact fitted drive unknown |
| Memory slots | 2× DDR2 SODIMM; up to 4 GB on GME965/PM965 models | Model-family fact; exact module layout unknown |
| Ports | 3× USB 2.0, VGA, RJ-45, audio in/out, SD/MMC, ExpressCard/34; modem optional | Model-family fact; physical condition unverified |
| Battery | 6-cell 47 Wh standard; 8-cell 63 Wh option existed | Exact installed battery/health unknown |
| AC adapter | 65 W class supported; VE908PA parts listings also show compatible 90 W adapters | Exact adapter currently paired with Laptop #10 unknown |
| RTC battery | 3 V lithium RTC battery | Model-family fact; condition unknown |

### CPU details relevant to Josh OS

The installed T5870 is a Merom-generation Core 2 Duo with:

- 2.0 GHz clock;
- 2 cores / 2 threads;
- 2 MB L2 cache;
- 800 MHz FSB;
- Intel 64 / x86-64;
- Execute Disable / NX support;
- Enhanced SpeedStep;
- no modern AVX/SSE4-era assumptions should be made.

Josh OS must therefore keep its early x86-64 baseline conservative and avoid accidentally compiling mandatory instructions newer than this CPU.

### Storage observations

The current drive model **HTS543212L9A300** is a Hitachi Travelstar 5K320:

- 120 GB decimal capacity / about 111.7 GiB binary capacity;
- SATA 3 Gb/s interface;
- 5400 rpm;
- 512-byte sectors / not Advanced Format;
- no TRIM.

Previous DadLAN measurements showed this machine's storage as extremely slow, reinforcing the decision not to optimise Josh OS around HDD performance characteristics. For initial Josh OS work, boot from removable media and leave the disk untouched.


## Why this machine

The Compaq 610 is useful as the first target because it is:

- an expendable DadLAN development machine rather than a primary computer;
- x86-64 capable, so it can run the canonical Josh kernel;
- old enough to exercise the legacy-PC path that JoshBootloader already researches;
- equipped with an internal keyboard that is likely to expose the early PS/2 path;
- simple enough to make initial hardware bring-up observable.

Those are development advantages, not claims of current compatibility.

## Exact-unit unknowns still to capture

Before Laptop #10 can move from **Selected** to **Inventoried**, capture these from the physical machine:

### Identity and firmware

- [ ] photograph/read the bottom service tag and confirm product number **VE908PA#ABG**;
- [ ] serial number recorded privately (do not publish it in the public repo);
- [ ] current BIOS version, date and ROM family;
- [ ] current BIOS configuration and boot order;
- [ ] whether any firmware password is set;
- [ ] SMBIOS system-board manufacturer/product/version;
- [ ] exact motherboard silkscreen / PCB revision;
- [ ] exact EC/super-I/O identity;
- [ ] exact SPI flash chip marking and board reference designator;
- [ ] firmware write-protect state.

### CPU, memory and chipset

- [ ] CPUID family/model/stepping;
- [ ] complete CPUID feature flags from this exact CPU;
- [ ] northbridge PCI ID;
- [ ] southbridge PCI ID;
- [ ] exact RAM module sizes, manufacturers, speeds and SPD data;
- [ ] confirm whether 3 GB is 2 GB + 1 GB and whether both slots are healthy.

### Display and graphics

- [ ] panel manufacturer/model and EDID;
- [ ] confirm native 1366×768 mode;
- [ ] confirm LED vs CCFL and anti-glare vs BrightView;
- [ ] exact Intel graphics PCI ID;
- [ ] VBE modes exposed by the vendor BIOS;
- [ ] framebuffer address/stride/pixel format actually handed to Limine/JoshBootloader.

### Storage

- [ ] HDD serial/firmware revision recorded privately where appropriate;
- [ ] SMART health, reallocated/pending sectors and power-on hours;
- [ ] SATA controller PCI ID and controller mode (IDE/AHCI if selectable);
- [ ] optical-drive exact model and interface;
- [ ] whether any original recovery partition still exists.

### Networking and radios

- [ ] wired NIC exact PCI ID and MAC recorded privately;
- [ ] fitted Wi-Fi card make/model/PCI ID;
- [ ] Wi-Fi antenna count;
- [ ] Bluetooth present/absent and USB ID;
- [ ] modem present/absent.

### Input and peripherals

- [ ] keyboard controller/i8042 behaviour;
- [ ] touchpad make/model/protocol;
- [ ] webcam present/absent and USB ID;
- [ ] SD/MMC reader controller ID;
- [ ] ExpressCard controller ID;
- [ ] exact USB controller PCI IDs and topology;
- [ ] speaker/audio codec exact ID;
- [ ] battery model, design capacity and current health;
- [ ] charger voltage/wattage actually in use;
- [ ] RTC battery condition.

### ACPI and power

- [ ] dump ACPI RSDP/RSDT/XSDT and table list;
- [ ] DSDT/SSDT capture;
- [ ] FADT shutdown/reboot behaviour;
- [ ] APIC/MADT contents;
- [ ] HPET availability;
- [ ] lid switch, battery and thermal-zone ACPI behaviour;
- [ ] cold-boot and warm-reboot behaviour.

### Physical Josh OS evidence

- [ ] Ventoy/USB boot-menu behaviour;
- [ ] Limine native ISO boot result;
- [ ] exact last successful serial marker;
- [ ] framebuffer success/failure;
- [ ] built-in keyboard success/failure;
- [ ] memory-map sanity;
- [ ] reboot behaviour;
- [ ] Stage 0 Linux-backed ISO boot result;
- [ ] repeated cold-boot results;
- [ ] repeated warm-reboot results.


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
