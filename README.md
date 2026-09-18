# JoshBIOS

**JoshBIOS is a from-scratch PC boot stack by Parris Tech Services.**

The long-term goal is to own the path from power-on firmware through the bootloader and into a JoshBIOS kernel. The first milestone deliberately starts with the part we can make generic and test safely: a real BIOS boot sector, our own Stage 2 loader and our own freestanding kernel.

## Josh OS ecosystem

This repository owns the **JoshBIOS / firmware / bootloader research stack**.

Related repositories:

- **Canonical Josh OS:** [joshuaparris-max/AshFallen](https://github.com/joshuaparris-max/AshFallen) — product track plus the independent x86-64 Josh kernel.
- **Stage 0 desktop/ISO extraction:** [Parris-Tech-Services/JoshOS-Stage0](https://github.com/Parris-Tech-Services/JoshOS-Stage0) — browser shell + ArchISO compatibility image.

Within this repository:

- **JoshBootloader** lives in [`boot/`](./boot)
- **JoshFirmware** development lives in [`firmware/`](./firmware)
- [`kernel/`](./kernel) is a **small boot-stack test kernel/payload**, not the canonical Josh OS kernel

```text
legacy BIOS integration path today:

platform BIOS / SeaBIOS
        ↓
JoshBIOS Stage 1
        ↓
JoshBootloader Stage 2
        ↓
MBR partition → FAT32
        ↓
/BOOT/JOSH/KERNEL.ELF
        ↓
ELF64 load → x86-64 long mode
        ↓
Josh Boot Protocol
        ↓
canonical AshFallen x86-64 kernel

The small local JoshBIOS kernel remains as an unpartitioned regression payload.

UEFI scaffold today:

UEFI / QEMU OVMF
        ↓
EFI/BOOT/BOOTX64.EFI
        ↓
Josh-owned x86-64 UEFI entry
        ↓
serial proof: JOSHUEFI_ENTRY_OK

canonical Josh OS native path:

BIOS / UEFI
        ↓
Limine
        ↓
AshFallen/kernel (x86-64 Josh kernel)

future convergence target:

JoshFirmware / platform firmware
        ↓
JoshBootloader
        ↓
canonical Josh OS x86-64 kernel
```

The convergence point should be a **versioned boot ABI** rather than copying kernels between repositories. JoshBootloader will need ELF64/x86-64 loading plus a hand-off containing the memory map, framebuffer, firmware data and other boot information expected by the canonical Josh OS kernel.

## Detailed roadmaps

- [Full-stack programming plan](docs/FULL_STACK_PROGRAMMING_PLAN.md) — all 37 phases from power button to applications, including JoshEC/custom-hardware endgame.
- [Difficulty, scope and feasibility](docs/DIFFICULTY_SCOPE_AND_FEASIBILITY.md) — what is tractable, brutal, blocked by platform access, and where AI assistance stops being strong.
- [Overall roadmap](ROADMAP.md)
- [JoshFirmware / JoshBIOS roadmap](docs/FIRMWARE_ROADMAP.md)
- [First physical hardware target — Compaq 610](docs/HARDWARE_TARGET_COMPAQ_610.md) — selected development machine, validation evidence and firmware recovery gates.
- [JoshBootloader roadmap](docs/BOOTLOADER_ROADMAP.md)
- [Josh Boot Protocol implementation plan](docs/JOSH_BOOT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)

The canonical kernel-side boot contract and whole-stack roadmap live in AshFallen.

## What works now

```text
PC reset
  -> platform BIOS / QEMU SeaBIOS
  -> JoshBIOS Stage 1 (512-byte boot sector, EDD/LBA disk read)
  -> JoshBootloader Stage 2
  -> MBR partition discovery
  -> FAT32 mount + /BOOT/JOSH/KERNEL.ELF lookup
  -> ELF64 validation/load
  -> E820 + VBE + ACPI/SMBIOS discovery
  -> x86-64 page tables + long-mode transition
  -> Josh Boot Protocol v0 hand-off
  -> canonical AshFallen kernel
  -> JOSHOS_BOOT_OK
```

No GRUB and no Linux kernel are involved in this native legacy-BIOS path. CI now proves the filesystem-backed JoshBootloader path reaches the canonical AshFallen kernel's `JOSHOS_BOOT_OK` marker in QEMU. The tiny local kernel remains only as a regression payload.

The UEFI path is now **tested scaffolding**, not a kernel boot path: CI builds a real x86-64 PE32+ `BOOTX64.EFI`, places it at the standard removable-media path on a FAT image, boots it under QEMU/OVMF, and requires the `JOSHUEFI_ENTRY_OK` serial marker. It does **not** yet discover GOP, capture the UEFI memory map, call `ExitBootServices`, load ELF64, construct the full Josh Boot Protocol, or enter AshFallen.

## Build

Requirements on a Debian/Ubuntu-style system:

```bash
sudo apt install build-essential binutils qemu-system-x86 clang lld mtools ovmf
make
make run
make uefi-image
make uefi-smoke
```

`make` produces:

```text
build/joshbios.img
```

That image is a 1 MiB raw boot disk suitable for QEMU. `make run` boots it with `qemu-system-i386`.

## Repository layout

```text
boot/               Stage 1 and Stage 2 bootloader
kernel/             Freestanding JoshBIOS kernel
firmware/coreboot/  Plan for true power-on firmware integration
docs/               Architecture notes
.github/workflows/  CI build
ROADMAP.md           Development path to x86_64 + real firmware support
```

## About “BIOS” and firmware

The code in `boot/` is JoshBIOS-owned code that runs after a legacy PC BIOS hands control to the boot sector. A genuine replacement for the firmware that executes immediately after the CPU resets cannot be one universal ROM: hardware initialisation is motherboard-specific.

The firmware track therefore targets **coreboot on explicitly supported boards**. That is how JoshBIOS can eventually participate in the path from the power button onward without pretending it is safe to flash arbitrary PCs.

## Safety

Do **not** flash experimental firmware to a physical motherboard. The current boot image belongs in QEMU or another disposable VM. Physical firmware support will require exact-board support and a recovery programmer. The first selected development system is DadLAN Laptop #10 / Compaq 610, but it is **not yet a supported or safe-to-flash JoshFirmware target**; see the hardware target document for the required inventory and recovery gates.

## Next milestone

Keep Limine as the independent reference path, then harden the now-working JoshBootloader path: explicitly assert ACPI/SMBIOS hand-off in CI, add typed filesystem/ELF failure fixtures, and prepare the legacy-BIOS image for the first non-destructive Compaq 610 USB boot test. Physical hardware support is not yet claimed.
