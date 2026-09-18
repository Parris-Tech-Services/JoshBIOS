# Claude boot-stack recovery branch

Saved urgently on 2026-09-18 so the work is recoverable if the session ends.

Branch: `verify/claude-boot-stack`

Preserved source files from Josh's uploaded Claude bundle:

- `boot/fat32.c`
- `boot/fat32.h`
- `boot/elf64.c`
- `boot/elf64.h`
- `boot/ata.c`
- `boot/ata.h`
- `boot/longmode.S`

Original uploaded archive SHA-256 reported during verification:

`85952b3c5abe42b19926d524e10c2a76769bde7ca27d571e843c4507935b456b`

Verification state at save time:

- ELF64 host tests were independently reproduced as 18/18 passing.
- FAT32 bundle tests were not independently rerun locally because the current sandbox lacked `mkfs.vfat` / mtools.
- ATA PIO and long-mode sources are preserved but not runtime-verified.
- This branch must not be merged to `main` until CI/test integration is completed.
- Existing JoshBIOS `main` was intentionally left untouched by these unverified files.

The uploaded bundle also contains host tests, image-generation helpers, a test Makefile, CI material and status notes which still need to be copied/integrated if not already present when work resumes.
