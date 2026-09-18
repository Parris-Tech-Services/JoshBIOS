# JoshBIOS firmware target

A true power-on firmware replacement is motherboard-specific. RAM training, chipset setup, CPU bring-up, PCI enumeration and platform security happen before a normal bootloader can run.

JoshBIOS therefore separates the project into two tracks:

1. **Bootable development stack (working now):** a JoshBIOS Stage 1 boot sector, Stage 2 loader and freestanding kernel that can run under a legacy BIOS such as QEMU/SeaBIOS.
2. **Power-on firmware track:** integrate JoshBIOS as a payload for **coreboot** on explicitly supported hardware. This lets coreboot perform board-specific hardware initialisation while JoshBIOS owns the user-visible firmware environment and boot policy.

## Safety rule

Do not flash JoshBIOS to physical hardware until that exact board is supported, a known-good firmware image has been backed up, and an external recovery programmer is available.

## Planned firmware milestones

- Define a small firmware-to-loader hand-off structure.
- Add a coreboot/libpayload console prototype.
- Add PCI/device inventory and memory-map discovery.
- Add boot-device selection and recovery mode.
- Support one known development board before considering broader hardware.
- Add signed/reproducible firmware images and rollback documentation.
