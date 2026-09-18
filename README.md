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
today:

platform BIOS / SeaBIOS
        ↓
JoshBIOS Stage 1
        ↓
JoshBootloader Stage 2
        ↓
JoshBIOS test kernel

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

- [Overall roadmap](ROADMAP.md)
- [JoshFirmware / JoshBIOS roadmap](docs/FIRMWARE_ROADMAP.md)
- [JoshBootloader roadmap](docs/BOOTLOADER_ROADMAP.md)
- [Josh Boot Protocol implementation plan](docs/JOSH_BOOT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)

The canonical kernel-side boot contract and whole-stack roadmap live in AshFallen.

## What works now

```text
PC reset
  -> platform BIOS / QEMU SeaBIOS
  -> JoshBIOS Stage 1 (512-byte boot sector, EDD/LBA disk read)
  -> JoshBIOS Stage 2 Boot Manager
       -> timed boot
       -> diagnostics
       -> reboot
  -> versioned JoshBootInfo v1 development hand-off
  -> A20 + GDT + 32-bit protected mode
  -> boot-stack test kernel
  -> serial boot proof + direct VGA output
```

No GRUB. No Linux kernel. No operating-system runtime. The bootloader and test payload are ours, and CI now proves the image actually reaches its boot-success marker in QEMU.

## Build

Requirements on a Debian/Ubuntu-style system:

```bash
sudo apt install build-essential binutils qemu-system-x86
make
make run
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

Do **not** flash experimental firmware to a physical motherboard. The current boot image belongs in QEMU or another disposable VM. Physical firmware support will require exact-board support and a recovery programmer.

## Next milestone

Keep the local kernel as a tiny regression payload and make **JoshBootloader boot the real AshFallen x86-64 kernel**: add filesystem-backed loading, ELF64 parsing, long-mode hand-off, serial diagnostics and the versioned Josh Boot Protocol. Limine remains the reference boot path until JoshBootloader reaches equivalent reliability.
