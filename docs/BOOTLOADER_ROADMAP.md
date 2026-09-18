# JoshBootloader roadmap

JoshBootloader is the narrow layer between firmware and the canonical Josh kernel.

Its job is to find and validate a boot entry, prepare the machine and boot information, then get out of the way.

The bootloader must not become a miniature operating system.

## Current state

The current experimental path:

```text
legacy BIOS
    ↓
512-byte Stage 1
    ↓
Stage 2
    ↓
A20 + GDT
    ↓
32-bit protected mode
    ↓
small test kernel
```

This proves ownership of a basic boot chain.

The next target is the real system:

```text
JoshFirmware / BIOS / UEFI
    ↓
JoshBootloader
    ↓
ELF64 canonical Josh kernel
    ↓
Josh Boot Protocol
    ↓
AshFallen/kernel
```

Limine remains the reference boot path while this matures.

---

# B0 — Preserve the tiny path as a test

Do not delete the simple 32-bit payload immediately.

Keep it as:

- boot-sector regression test;
- minimal loader smoke target;
- fallback way to isolate bootloader bugs from kernel bugs.

Rename/document it clearly as a **test payload**, never the canonical Josh kernel.

---

# B1 — Disk and filesystem layer

Replace fixed-sector assumptions.

- [ ] block-read abstraction;
- [ ] BIOS disk path isolated behind an adapter;
- [ ] UEFI file/block path later;
- [ ] MBR/GPT partition discovery;
- [ ] FAT32 reader;
- [ ] path lookup;
- [ ] file size/range validation;
- [ ] robust short-read/error handling.

Initial layout should be deliberately boring and documented.

Example:

```text
/EFI/JOSH/...
/boot/josh/kernel.elf
/boot/josh/recovery.elf
/boot/josh/boot.cfg
```

Exact paths are not frozen yet.

---

# B2 — ELF64 loader

Must correctly:

- [ ] validate ELF magic/class/endianness/machine;
- [ ] inspect program headers;
- [ ] reject overlapping/invalid ranges;
- [ ] load PT_LOAD segments;
- [ ] zero BSS;
- [ ] honour alignment;
- [ ] track loaded physical/virtual ranges;
- [ ] expose entry point;
- [ ] reject unsupported relocation models explicitly.

Add host-side parser tests and malformed-image fixtures.

---

# B3 — x86-64 hand-off

- [ ] verify long-mode CPU support;
- [ ] build required page tables;
- [ ] enable PAE/long mode/paging in correct sequence;
- [ ] install temporary GDT;
- [ ] establish known stack;
- [ ] establish known register contract;
- [ ] jump to canonical kernel entry.

Document exact machine state at hand-off.

Do not rely on accidental register values.

---

# B4 — Josh Boot Protocol

Implement the versioned contract documented in AshFallen `docs/BOOT_ABI.md`.

Initial fields:

- ABI version;
- memory map;
- framebuffer;
- ACPI RSDP;
- SMBIOS;
- boot device;
- command line;
- modules/initrd;
- entropy;
- boot health;
- firmware security state.

The bootloader constructs it; the kernel validates it.

---

# B5 — Firmware adapters

Support multiple front ends without contaminating the core loader.

## Legacy BIOS adapter

- BIOS disk reads;
- E820 memory map;
- VBE/framebuffer if used;
- ACPI discovery;
- boot drive identity.

## UEFI adapter

- file/block access;
- GOP framebuffer;
- UEFI memory map;
- ACPI/SMBIOS tables;
- loaded-image/device path;
- final ExitBootServices sequence.

Core ELF/config/menu logic should be shared.

---

# B6 — Boot configuration

Create a small versioned configuration format.

Needs:

- entry ID;
- display name;
- kernel path;
- command line;
- modules/initrd;
- default;
- timeout;
- flags such as recovery/development.

Avoid a programming language in the boot config.

Parser must fail closed on malformed critical values.

---

# B7 — Boot menu

Yes: JoshBootloader should have a **boot menu**.

## Normal path

- default entry;
- short configurable timeout or immediate boot;
- menu hidden unless requested.

## Reveal menu when

- user holds/presses menu key;
- previous boot failed;
- pending update needs validation;
- recovery has been requested;
- config is ambiguous/invalid.

## Initial entries

- Josh OS
- Previous known-good
- Recovery
- Diagnostics

Later:

- development kernel;
- alternate installed OS;
- chainload entry;
- external entry where firmware policy permits.

## UI principles

- keyboard-first;
- readable on low-resolution framebuffer;
- no dependency on GPU acceleration;
- no elaborate animation;
- always show build/version in diagnostics;
- verbose mode available.

---

# B8 — Boot health and previous-known-good

Persist minimal boot state:

- selected entry ID;
- attempt counter;
- pending-good flag;
- last successful entry;
- last failure stage;
- update generation.

Flow:

1. new kernel/base-OS build installed;
2. bootloader marks it pending;
3. system boots;
4. userspace reaches explicit healthy checkpoint;
5. system marks build good;
6. repeated failure exposes/reverts to previous-good.

Do not mark success merely because the kernel entry point was reached.

---

# B9 — Recovery entry

Recovery must be deliberately smaller than the normal OS.

Bootloader responsibilities:

- locate recovery kernel/image;
- verify it;
- load it;
- pass recovery reason/boot health.

Recovery environment responsibilities belong above the bootloader.

---

# B10 — Kernel/image verification

Before enforcing signatures everywhere, build the plumbing:

- [ ] hash kernel/modules;
- [ ] display hashes in diagnostics;
- [ ] optional signature metadata;
- [ ] verification result in Josh Boot Protocol;
- [ ] development override clearly marked;
- [ ] later release-key enforcement.

Never silently boot an image that failed a policy-required verification.

---

# B11 — Measured boot

Where TPM exists:

- measure bootloader config/kernel/modules;
- extend defined PCRs;
- expose event log pointer/data;
- pass state to kernel.

Measured boot records what was loaded; signature verification decides whether it is allowed. Keep these concepts distinct.

---

# B12 — Chainloading / multi-boot

Only after Josh OS boot is solid.

Possible future support:

- UEFI executable chainload;
- another installed OS entry;
- removable media delegation to firmware.

Do not become a GRUB clone unless users actually need that scope.

---

# B13 — Diagnostics

Verbose screen/serial should show:

- firmware adapter;
- boot device;
- selected entry;
- config path;
- kernel path/size/hash;
- ELF segments;
- memory-map summary;
- framebuffer;
- ACPI pointer;
- Boot Protocol version;
- hand-off address.

Normal mode remains quiet.

---

# B14 — Error handling

Every load stage returns a typed/result code.

Examples:

- no config;
- no default entry;
- disk read failed;
- filesystem corrupt;
- file missing;
- ELF invalid;
- architecture unsupported;
- out of memory/address range;
- framebuffer unavailable;
- memory map unavailable;
- protocol construction failed;
- verification failed.

Error UI should provide a recovery/menu action rather than “halt”.

---

# B15 — Tests

## Host tests

- config parser;
- GPT/MBR parser;
- FAT32 parser;
- ELF64 parser;
- Boot Protocol builder;
- malformed/corrupt fixtures.

## QEMU matrix

- legacy BIOS;
- UEFI/OVMF;
- multiple RAM sizes;
- multiple framebuffer sizes;
- missing kernel;
- corrupt ELF;
- corrupt config;
- recovery entry;
- previous-good fallback.

## Integration contract

CI should boot the **canonical AshFallen kernel**, not only the local test payload.

---

# Definition of first real JoshBootloader milestone

JoshBootloader v0.x becomes meaningfully integrated when it can:

1. boot in QEMU from legacy BIOS;
2. locate a filesystem rather than hard-coded sectors;
3. parse/load the canonical Josh ELF64 kernel;
4. enter x86-64 long mode;
5. pass memory map + framebuffer through Josh Boot Protocol;
6. reach the AshFallen kernel's boot-success marker;
7. expose a recovery/menu path;
8. fail visibly and diagnostically;
9. repeat this in CI.

After that, add the UEFI adapter and broaden recovery/security.
