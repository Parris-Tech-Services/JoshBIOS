# Claude boot-stack import checkpoint — 18 Sep 2026

Branch: `verify/claude-boot-stack`

This branch is an engineering checkpoint for the uploaded `josh-boot-stack.zip`. It is deliberately **not merged to main yet**.

## What has been independently checked

- Bundle contents inspected: FAT32 reader, ELF64 loader, ATA PIO, long-mode transition, host tests and CI.
- ELF64 host suite was independently reproduced locally: **18/18 checks passed**.
- Freestanding compile checks for FAT32, ELF64, ATA and long-mode sources passed with `-Werror`.
- Existing JoshBIOS BIOS smoke path remains green in GitHub Actions on the verification branch.
- Existing JoshBIOS UEFI/OVMF entry path remains green in GitHub Actions on the verification branch.

## What is still unverified

- FAT32 host suite has not yet completed in GitHub Actions because the temporary source-transfer archive was corrupted while being reconstructed from the first large text chunk.
- ATA PIO has compiled but has not executed in QEMU or on hardware.
- `longmode.S` has compiled but has not executed; no page-table builder exists yet.
- No Stage 3 wiring exists yet to connect FAT32 -> ELF64 -> long mode -> canonical AshFallen kernel.
- No claim should be made that JoshBootloader boots AshFallen yet.

## Transfer failure found

The first GitHub Actions import attempt used a binary blob upload and produced a malformed zip.
The second attempt used base64 text chunks and narrowed the remaining corruption to the original `chunk-00`; the other chunk Git blobs matched their expected local hashes.

The original first chunk has now been split into two smaller checkpoint blobs so the next agent can replace the damaged chunk without regenerating the whole bundle.

## Next exact step

1. Replace/remove the damaged `.ci/import/chunk-00` with `chunk-00a` + `chunk-00b`.
2. Ensure the reconstruction command concatenates chunks in exact order.
3. Run `unzip -t`.
4. Run `make -f Makefile.tests test`.
5. Run `make -f Makefile.tests buildcheck`.
6. Preserve `make smoke` and `make uefi-smoke`.
7. Only after all of those are green, commit the extracted source and merge the verified code to `main`.
8. Update the existing bootloader roadmap/status docs with truthful labels:
   - FAT32: implemented/tested only if the FAT32 suite passes;
   - ELF64: implemented/tested;
   - ATA: implemented/builds, not tested;
   - long mode: scaffolded/builds, not tested and incomplete.

Do not merge the temporary `.ci/import/` transfer payloads or `.github/workflows/import-boot-stack.yml` to `main`; they are only for this verification handoff.
