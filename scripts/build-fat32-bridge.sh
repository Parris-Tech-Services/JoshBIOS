#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <output.img> <stage1.bin> <stage2.bin> <kernel.elf>" >&2
  exit 2
fi

image=$1
stage1=$2
stage2=$3
kernel=$4

total_bytes=$((64 * 1024 * 1024))
total_sectors=$((total_bytes / 512))
partition_lba=2048
partition_sectors=$((total_sectors - partition_lba))
partition_offset=$((partition_lba * 512))

for file in "$stage1" "$stage2" "$kernel"; do
  test -f "$file" || { echo "missing input: $file" >&2; exit 1; }
done

stage1_size=$(wc -c < "$stage1")
test "$stage1_size" -eq 512 || {
  echo "stage1 must be exactly 512 bytes, got $stage1_size" >&2
  exit 1
}

rm -f "$image"
truncate -s "$total_bytes" "$image"
dd if="$stage1" of="$image" conv=notrunc status=none
dd if="$stage2" of="$image" bs=512 seek=1 conv=notrunc status=none

python3 - "$image" "$partition_lba" "$partition_sectors" <<'PY'
import struct
import sys

path = sys.argv[1]
first_lba = int(sys.argv[2])
sector_count = int(sys.argv[3])

with open(path, "r+b") as f:
    mbr = bytearray(f.read(512))
    if len(mbr) != 512 or mbr[510:512] != b"\x55\xaa":
        raise SystemExit("stage1 MBR signature missing")

    entry = bytearray(16)
    entry[0] = 0x80
    entry[4] = 0x0C
    struct.pack_into("<I", entry, 8, first_lba)
    struct.pack_into("<I", entry, 12, sector_count)
    mbr[446:462] = entry
    mbr[462:510] = b"\x00" * 48
    mbr[510:512] = b"\x55\xaa"

    f.seek(0)
    f.write(mbr)
PY

mformat -i "${image}@@${partition_offset}" -F -v JOSHBOOT ::
mmd -i "${image}@@${partition_offset}" ::/BOOT
mmd -i "${image}@@${partition_offset}" ::/BOOT/JOSH
mcopy -i "${image}@@${partition_offset}" "$kernel" ::/BOOT/JOSH/KERNEL.ELF

python3 - "$image" <<'PY'
import sys

path = sys.argv[1]
with open(path, "rb") as f:
    f.seek(33 * 512)
    raw_fallback = f.read(512 * 512)
if any(raw_fallback):
    raise SystemExit("raw fallback extent is not empty; FAT32 smoke would be ambiguous")
PY

echo "FAT32 bridge image ready: $image"
echo "  partition LBA: $partition_lba"
echo "  kernel path: /BOOT/JOSH/KERNEL.ELF"
