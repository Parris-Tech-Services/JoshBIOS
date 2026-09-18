# JoshBootloader roadmap

JoshBootloader is the narrow layer between firmware and the canonical Josh kernel.

Its job is to find and validate a boot entry, prepare the machine and boot information, then get out of the way.

The bootloader must not become a miniature operating system.

## Current state

The tiny fixed-extent 32-bit payload remains as a regression target, but the verified legacy-BIOS integration path is now:

```text
legacy BIOS
    ↓
512-byte Stage 1
    ↓
Stage 2
    ↓
MBR partition discovery
    ↓
FAT32
    ↓
/boot/josh/kernel.elf
    ↓
ELF64 loader
    ↓
x86-64 long mode
    ↓
Josh Boot Protocol
    ↓
canonical AshFallen kernel
```

GitHub CI boots this path in QEMU and requires the canonical kernel's `JOSHOS_BOOT_OK` marker. Limine remains the independent reference boot path.

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

- [x] block-read abstraction;
- [x] BIOS disk path isolated behind an INT 13h EDD adapter;
- [ ] UEFI file/block path later;
- [x] MBR/GPT partition discovery (GPT host-tested; MBR used by the integration image);
- [x] FAT32 reader;
- [x] 8.3 path lookup;
- [x] file size/range validation;
- [x] short-read/corruption/error handling.

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

- [x] validate ELF magic/class/endianness/machine;
- [x] inspect program headers;
- [x] reject overlapping/invalid ranges;
- [x] load PT_LOAD segments;
- [x] zero BSS;
- [x] honour alignment;
- [x] track loaded physical/virtual ranges;
- [x] expose entry point;
- [ ] reject unsupported relocation models explicitly.

Add host-side parser tests and malformed-image fixtures.

---

# B3 — x86-64 hand-off

- [x] verify long-mode CPU support;
- [x] build required page tables;
- [x] enable PAE/long mode/paging in correct sequence;
- [x] install temporary GDT;
- [x] establish known stack;
- [x] establish known register contract;
- [ ] jump to canonical kernel entry.

Document exact machine state at hand-off.

Stage2 now checks CPUID availability, PAE and the extended long-mode capability bit before touching long-mode control state. QEMU integration requires the `JOSHBOOT_CPU_LONG_MODE_OK` marker before hand-off.

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

# Historical implementation checkpoint — 18 Sep 2026 (superseded)

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

## Historical limitation at this checkpoint

At this point in the history, the Stage1/Stage2 path had not yet been switched to the ELF64/long-mode path. This limitation is **superseded by the verified integration checkpoints below** and is retained only to explain the sequence of work.

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

## Verified integration checkpoint — 18 Sep 2026

This section records observed behaviour, not planned work.

### JoshBootloader → canonical AshFallen boot is now verified in QEMU

The legacy-BIOS JoshBootloader path now:

1. loads Stage2 through BIOS EDD;
2. preloads the canonical AshFallen ELF64 kernel;
3. captures BIOS E820 memory-map data;
4. selects a 32-bpp VBE linear framebuffer;
5. validates and loads the ELF64 PT_LOAD segments;
6. zeros BSS and records loaded kernel bounds;
7. builds the experimental Josh Boot Protocol v0 structure;
8. builds bootstrap x86-64 page tables;
9. enables PAE, EFER.LME and paging;
10. enters 64-bit long mode;
11. passes the documented loader magic values plus JoshBootInfo pointer;
12. jumps to the canonical AshFallen kernel entry point.

GitHub Actions run `35343090145` on `main` observed, in order:

```text
JOSHBOOT_ELF64_DETECTED
JOSHBOOT_HANDOFF_READY
JOSHOS_KERNEL_ENTERED
JOSHOS_GDT_TSS_OK
JOSHOS_IDT_OK
JOSHOS_BOOT_ADAPTER_OK
JOSHOS_BOOT_OK
```

The QEMU bridge smoke test therefore proves the current JoshBootloader can enter the canonical x86-64 Josh kernel without Limine on this legacy-BIOS test path.

Relevant implementation commits include:

- `a897b5b` — live ELF64/long-mode hand-off;
- `b5e062b` — AshFallen bridge image + QEMU smoke target;
- `27f2a5e` — CI boots the canonical AshFallen kernel through JoshBootloader.

### Strongest truthful status

- ELF64 parser/loader: **implemented + host-tested + integrated in the QEMU bridge path**.
- x86-64 long-mode transition: **implemented + integration-tested in QEMU**.
- Josh Boot Protocol v0 kernel hand-off: **implemented + integration-tested in QEMU**.
- JoshBootloader → canonical AshFallen kernel: **verified in QEMU legacy-BIOS path**.
- Limine path: **still retained and working as the reference path**.
- Filesystem-based kernel discovery: **implemented and integration-tested** on the legacy-BIOS QEMU path using MBR → FAT32 → `/boot/josh/kernel.elf`.
- Generic hardware support: **not claimed**; this evidence is QEMU-specific.



---

## Verified filesystem integration checkpoint — 18 Sep 2026

GitHub Actions run `35344381859` verified that the canonical kernel is no longer dependent on a hard-coded kernel disk extent in the integration image.

The bridge image contains an MBR FAT32 partition beginning at LBA 2048 and stores the canonical kernel at:

```text
/BOOT/JOSH/KERNEL.ELF
```

The live Stage2 path uses the BIOS EDD block adapter, partition parser and FAT32 reader to locate and read that file into the ELF64 staging buffer.

The run observed, in order:

```text
JOSHBOOT_FS_LOAD_BEGIN
JOSHBOOT_PARTITION_OK
JOSHBOOT_FAT32_OK
JOSHBOOT_KERNEL_PATH_OK
JOSHBOOT_KERNEL_FILE_LOADED
JOSHBOOT_ELF64_DETECTED
JOSHBOOT_HANDOFF_READY
JOSHOS_KERNEL_ENTERED
JOSHOS_GDT_TSS_OK
JOSHOS_IDT_OK
JOSHOS_BOOT_ADAPTER_OK
JOSHOS_BOOT_OK
```

Strongest truthful labels:

- BIOS block adapter: **implemented + integration-tested in QEMU**;
- MBR discovery: **implemented + integration-tested in QEMU**;
- GPT discovery: **implemented + host-tested**, not yet used by the bridge image;
- FAT32 reader/path lookup: **implemented + host-tested + integration-tested in QEMU**;
- canonical kernel file loading: **implemented + integration-tested in QEMU**;
- JoshBootloader → canonical kernel via FAT32: **verified in QEMU legacy-BIOS path**;
- physical hardware support: **not yet verified**;
- UEFI loader: **scaffolded/boot-tested to entry only**, not yet a kernel loader.


---

## Verified platform-metadata checkpoint — 18 Sep 2026

GitHub Actions run `35345509864` requires and observed:

```text
JOSHBOOT_CPU_LONG_MODE_OK
JOSHBOOT_HANDOFF_READY
JOSHOS_RSDP_OK
JOSHOS_SMBIOS_OK
JOSHOS_BOOT_OK
```

This verifies that the current legacy-BIOS QEMU path checks the CPU before entering long mode and that the ACPI RSDP and SMBIOS pointers discovered by JoshBootloader survive the Josh Boot Protocol hand-off into the canonical AshFallen kernel.

This is QEMU evidence, not physical-hardware support.
