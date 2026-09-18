# Claude boot-stack recovery and reconciliation

Saved from Josh's uploaded `josh-boot-stack.zip` on 18 September 2026.

Branch: `verify/claude-boot-stack`

## Exact preservation

The original archive is preserved byte-for-byte as five chunks under `recovery/claude/`.
See `recovery/claude/MANIFEST.md` for reconstruction instructions and per-chunk hashes.

Original archive:

- size: 33,543 bytes
- SHA-256: `85952b3c5abe42b19926d524e10c2a76769bde7ca27d571e843c4507935b456b`

The exact source and test files were also extracted onto this branch. Their Git blob IDs were checked against local `git hash-object` results before promotion.

## Independent verification

GitHub Actions run **35344191821** reconstructed the archive from the preserved chunks, verified the archive SHA-256, tested the ZIP integrity, extracted it, ran the original host suites and ran the original freestanding buildcheck.

Observed results from the exact uploaded code:

- FAT32 superfloppy: **43 checks, 0 failed**
- FAT32 MBR-partitioned image: **43 checks, 0 failed**
- FAT32 host test compiled with AddressSanitizer + UndefinedBehaviorSanitizer
- ELF64: **18 checks, 0 failed**
- ATA PIO: compiles clean with the freestanding `-Werror` buildcheck
- FAT32 freestanding object: compiles clean with `-Werror`
- ELF64 freestanding object: compiles clean with `-Werror`
- `longmode.S`: assembles cleanly

The same commit also passed the repository's normal BIOS + UEFI CI.

## What is still not verified in Claude's bundle

- `boot/ata.c` has not been executed against QEMU or physical hardware.
- `boot/longmode.S` has not been executed.
- Claude's long-mode file does not build the page tables it requires; it expects a valid PML4 from a caller.
- The bundle does not by itself wire ATA + FAT32 + ELF64 + page tables + long mode into an end-to-end JoshBootloader boot of AshFallen.

Do not relabel those pieces as runtime-tested.

## Reconciliation with current main

While this bundle was being verified, another coding agent advanced `main` substantially.

Current `main` now has its own:

- bounded block-device abstraction;
- MBR + GPT discovery;
- FAT32 reader and 8.3 path lookup;
- ELF64 validator with segment-overlap, alignment and executable-entry checks;
- protected-mode loader integration;
- BIOS-backed block-read thunk;
- bootstrap page-table construction;
- x86-64 long-mode hand-off;
- Josh Boot Protocol hand-off;
- FAT32 image containing the canonical AshFallen kernel.

GitHub Actions run **35344381859** verified the integrated path and emitted:

```text
JOSHBOOT_PARTITION_OK
JOSHBOOT_FAT32_OK
JOSHBOOT_KERNEL_PATH_OK
JOSHBOOT_KERNEL_FILE_LOADED
JOSHBOOT_ELF64_DETECTED
JOSHBOOT_HANDOFF_READY
JOSHOS_KERNEL_ENTERED
JOSHOS_BOOT_ADAPTER_OK
JOSHOS_BOOT_OK
```

Therefore the Claude sources should **not** be copied wholesale over `main`.
They remain valuable as an independently tested reference implementation and test corpus.

Potential ideas worth harvesting later, only when useful:

- Claude's FAT32 tests use `mkfs.vfat` + `mcopy` as an independent filesystem writer and include a 1 MiB multi-cluster byte-for-byte test.
- Claude's FAT32 implementation includes VFAT long-filename lookup; current boot paths deliberately only require 8.3 names.
- Claude's ATA PIO implementation may be useful for a future direct-hardware storage adapter, but it is not a replacement for the currently verified BIOS block path and is not runtime-tested.

## Engineering status

**Preserved:** yes  
**Exact archive verified:** yes  
**Host tests verified:** yes  
**ATA runtime tested:** no  
**Claude long-mode runtime tested:** no  
**Merged wholesale to main:** no, deliberately  
**Reason:** current main already contains a newer integrated implementation with stronger end-to-end QEMU evidence.
