# Josh Boot Protocol — bootloader-side plan

The canonical specification direction lives with the Josh kernel in:

- `joshuaparris-max/JoshOS/docs/BOOT_ABI.md`

This document records the bootloader implementation responsibilities.

## Contract rule

JoshBootloader must not make the kernel depend on BIOS, UEFI, coreboot or any one firmware environment.

Firmware-specific information is converted into a common versioned structure before kernel entry.

## Current UEFI scaffold

As of 18 September 2026, the UEFI path has crossed its first executable milestone:

- `boot/uefi/main.c` builds as an x86-64 PE32+ EFI application;
- the build creates a FAT removable-media image containing `EFI/BOOT/BOOTX64.EFI`;
- GitHub Actions boots that image under QEMU/OVMF;
- the test requires the serial marker `JOSHUEFI_ENTRY_OK`.

Status: **tested scaffold**.

This does not yet mean JoshBootloader can boot Josh OS through UEFI. GOP discovery, `GetMemoryMap`, ACPI/SMBIOS table capture, filesystem/kernel loading, `ExitBootServices`, Josh Boot Protocol construction and JoshOS kernel entry are still unimplemented on this path.

## Bootloader implementation modules

Target conceptual split:

```text
firmware adapter
      ↓
block/filesystem
      ↓
boot config
      ↓
ELF loader
      ↓
platform memory/framebuffer discovery
      ↓
Josh Boot Protocol builder
      ↓
architecture hand-off
      ↓
kernel entry
```

## Proposed source boundaries

Eventually:

```text
boot/
  arch/x86_64/
  firmware/bios/
  firmware/uefi/
  fs/fat32/
  disk/
  elf/
  config/
  menu/
  protocol/
  recovery/
  security/
  diagnostics/
```

Names may change; responsibilities should not blur.

## Required invariants

Before jumping to kernel:

- kernel ELF validated;
- every loaded segment within allowed physical ranges;
- no critical segment overlap;
- stack valid;
- page tables valid;
- memory map captured at correct firmware lifecycle point;
- framebuffer validated;
- Boot Protocol magic/version set;
- required pointers physical/virtual semantics documented;
- optional fields marked absent explicitly;
- serial/diagnostic hand-off marker emitted.

## BIOS path

Translate:

- INT 13h/block reads → common storage abstraction;
- E820 → Josh memory map;
- VBE/framebuffer → common framebuffer if supported;
- ACPI scan → RSDP field.

## UEFI path

Translate:

- protocols/device handles → common boot device abstraction;
- GOP → common framebuffer;
- GetMemoryMap → common memory map;
- configuration tables → ACPI/SMBIOS;
- RNG protocol when available → entropy.

Capture the final memory map and perform `ExitBootServices` correctly before transferring control.

## Compatibility strategy

During transition:

1. Limine path remains canonical reference.
2. Kernel adds internal boot abstraction.
3. JoshBootloader implements protocol v0.
4. QEMU test boots same kernel both ways.
5. compare normalised memory/framebuffer data.
6. only then make JoshBootloader a default development path.

## Conformance tests

Create fixtures for:

- smallest valid structure;
- future-minor-version structure;
- unknown optional flags;
- missing required field;
- invalid sizes;
- overlapping memory entries;
- invalid framebuffer pitch;
- zero entropy;
- unsupported major version.

Both kernel and bootloader should test against the same fixture definitions where practical.
