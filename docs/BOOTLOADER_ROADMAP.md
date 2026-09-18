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


---

# Active implementation checkpoint — 18 Sep 2026

This section is a recovery checkpoint for concurrent coding agents. It records only code that currently exists on `main`.

## Implemented and pushed

- Josh Boot Protocol v0 layout exists in `boot/protocol.h`.
- Strict host-testable ELF64 parser exists in `boot/elf64.c` and `boot/elf64.h`.
- Malformed ELF64 tests exist in `tests/test_elf64.c`.
- `make host-tests` builds/runs the parser tests.
- GitHub CI invokes `make host-tests`.
- `boot/stage2_pm.c` now contains a protected-mode integration planner that:
  - detects the preloaded ELF64 image;
  - validates and copies PT_LOAD segments;
  - zeros BSS;
  - constructs Josh Boot Protocol v0 data;
  - consumes BIOS-provided E820/VBE buffers;
  - scans for ACPI RSDP and SMBIOS;
  - creates bootstrap identity + higher-half page tables;
  - prepares the canonical AshFallen entry address;
  - calls an external `enter_long_mode()` hand-off routine.

## Important limitation

The current working Stage1/Stage2 path has **not yet been switched** to the new ELF64/long-mode path. The existing 32-bit test payload remains the active boot target, so the new `stage2_pm.c` integration code is presently dormant.

Do not claim that JoshBootloader boots AshFallen yet.

## Exact next atomic change

Update Stage1/Stage2/Makefile together so that:

1. Stage1 reserves enough sectors for the larger Stage2 image.
2. Stage2 uses BIOS EDD to preload the canonical AshFallen ELF64 image into the agreed temporary physical buffer.
3. Stage2 captures E820 into the buffer consumed by `stage2_pm.c`.
4. Stage2 obtains a VBE linear framebuffer mode and fills the VBE buffer consumed by `stage2_pm.c`.
5. The 32-bit Stage2 build links `stage2_pm.c` and `elf64.c`.
6. Add `enter_long_mode` assembly that enables PAE, EFER.LME and paging, loads a 64-bit GDT, establishes a known stack/register contract, and jumps to `stage2_kernel_entry`.
7. Build the integration image using the real AshFallen `kernel/bin/kernel`.
8. QEMU must reach AshFallen's existing `JOSHOS_BOOT_OK` serial marker before this milestone is called integrated.

Keep Limine working as the reference path throughout.


---

## Recovery checkpoint — 18 Sep 2026

This section is an engineering hand-off, not a claim that the integration is finished.

### Landed on `main`

- `63bc0ad` — ELF64 parser interface.
- `29fbd68` — strict x86-64 ET_EXEC ELF validation.
- `b764fa4` — malformed ELF64 host-test fixtures.
- `76c4f41` — `make host-tests` target.
- `5ab1aed` — CI runs the ELF64 host tests.
- `a150484` — dormant protected-mode ELF64 load planner, Josh Boot Protocol builder, E820/VBE consumer, ACPI/SMBIOS discovery and bootstrap page-table builder in `boot/stage2_pm.c`.

AshFallen currently has the matching Josh Boot Protocol v0 adapter and host tests on its `main` branch (commit `529acf1` also added the IDT exception panic path). Limine remains the working reference path.

### What is implemented but not yet integrated

`boot/stage2_pm.c` currently contains substantive code for:

- validating the preloaded canonical ELF64 image;
- copying `PT_LOAD` segments into a physical kernel window;
- zeroing BSS;
- calculating canonical physical/virtual kernel bounds;
- building a v0 `JoshBootInfo`;
- translating BIOS E820 entries already captured by Stage2;
- consuming VBE framebuffer information already captured by Stage2;
- scanning for ACPI RSDP and SMBIOS entry points;
- building bootstrap x86-64 page tables;
- preparing the canonical kernel entry address.

This code compiles as 32-bit freestanding C, but it is **dormant**: current Stage2 still boots the small local 32-bit test payload. Do not call the JoshBootloader→AshFallen path integrated yet.

### Next atomic change

The next commit should update Stage1, Stage2 and the Makefile together so the live BIOS path can safely exercise `stage2_pm.c` without leaving `main` half-switched:

1. grow Stage2 allocation from 16 to 32 sectors;
2. move the preloaded kernel image start from LBA 17 to LBA 33;
3. preload up to 512 sectors of the canonical ELF64 image at physical `0x10000`;
4. collect E820 into the shared v0 memory-map layout at `0x59000`;
5. request/set a 32-bpp VBE linear framebuffer and retain the mode info at `0x5a000`;
6. enter protected mode and call `stage2_pm_main`;
7. add the assembly `enter_long_mode` routine that enables PAE, EFER.LME and paging, loads the 64-bit GDT, installs a known stack, and jumps to `stage2_kernel_entry`;
8. pass `JOSH_LOADER_MAGIC1`, `JOSH_LOADER_MAGIC2`, and the `JoshBootInfo *` in the exact registers expected by the AshFallen `kmain`;
9. make a new integration image by copying AshFallen's canonical `kernel/bin/kernel` into the disk image;
10. require `JOSHOS_KERNEL_ENTERED`, `JOSHOS_BOOT_ADAPTER_OK`, and `JOSHOS_BOOT_OK` in QEMU CI.

If the canonical-kernel path fails, preserve the existing local 32-bit payload as an explicit regression target rather than deleting it.

### Current strongest truthful labels

- ELF64 parser: **implemented + host-tested**.
- Josh Boot Protocol layout: **implemented on both sides + kernel host-tested**.
- protected-mode ELF64 load/page-table planner: **implemented, not integrated**.
- JoshBootloader → canonical AshFallen boot: **not yet working / not yet verified**.
