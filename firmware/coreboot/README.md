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


## HP Compaq 610 physical target

The current physical candidate is Laptop #10, HP Compaq 610 SKU `VE908PA#ABG`.
External evidence points to the Intel UMA Vulcain/VV09 family with HP system
board spare `538409-001`, candidate board silkscreen
`VV09-6050A2256501-MB-A04`, Intel GM/GME965 + ICH8M, U23
`SST25VF080B` (1 MiB, 3.3 V) and SMSC KBC1070. These are **candidate
identifiers**, not yet observations from Laptop #10.

Current coreboot contains GM965 northbridge support and ICH8M-family southbridge
support; Lenovo X61 is the reference mainboard using that stack. The Compaq
board-specific EC, GPIO, SPD routing, clock, display and power sequencing must
come from the target hardware capture rather than being copied from X61.

Use `firmware/coreboot/compaq610/probe-linux.sh` on the physical laptop for
live DMI/PCI/chipset evidence only; it deliberately does not access the SPI
flash. After powering the target down, use
`capture-spi-external.sh <probe-dir> <programmer>` from the external-programmer
host to obtain three byte-identical OEM ROM reads. Then run `preflight.py`.
Physical motherboard/SPI markings and external-capture provenance are mandatory
inputs. `flash-gate.py` additionally requires a proven external OEM-ROM restore
before any future physical flash backend may be enabled.

No repository target flashes the Compaq 610.
