# JoshBIOS

**JoshBIOS is a from-scratch PC boot stack by Parris Tech Services.**

The long-term goal is to own the path from power-on firmware through the bootloader and into a JoshBIOS kernel. The first milestone deliberately starts with the part we can make generic and test safely: a real BIOS boot sector, our own Stage 2 loader and our own freestanding kernel.

## JoshOS ecosystem

JoshBIOS is the low-level boot stack for [JoshOS](https://github.com/Parris-Tech-Services/JoshOS).

```text
Power button
   ↓
JoshFirmware
   ↓
JoshBIOS
   ↓
JoshBootloader
   ↓
JoshOS
```

Within this repository:

- **JoshBootloader** lives in [`boot/`](./boot)
- **JoshFirmware** development lives in [`firmware/`](./firmware)
- the experimental low-level kernel lives in [`kernel/`](./kernel)
- the higher-level OS project lives in [Parris-Tech-Services/JoshOS](https://github.com/Parris-Tech-Services/JoshOS)
- the Stage 0 Josh OS shell/ISO implementation lives in [Parris-Tech-Services/transfer2](https://github.com/Parris-Tech-Services/transfer2)

## What works now

```text
PC reset
  -> platform BIOS / QEMU SeaBIOS
  -> JoshBIOS Stage 1 (512-byte boot sector)
  -> JoshBIOS Stage 2
  -> A20 + GDT + 32-bit protected mode
  -> JoshBIOS kernel
  -> direct VGA output
```

No GRUB. No Linux kernel. No operating-system runtime. The bootloader and kernel are ours.

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

Move the kernel to x86_64 long mode, add exceptions/interrupts, keyboard input, serial debugging and a versioned firmware/bootloader-to-kernel hand-off structure.
