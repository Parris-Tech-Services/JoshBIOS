#!/usr/bin/env bash
#
# make-test-image.sh - Build FAT32 images for the driver tests.
#
# Deliberately uses mkfs.vfat and mcopy (dosfstools + mtools) so the
# filesystem under test is produced by an independent implementation.
# Testing our reader against our own writer would prove very little.
#
set -euo pipefail

OUT="${1:-build}"
mkdir -p "$OUT"

IMG="$OUT/test.img"
PART_IMG="$OUT/test-part.img"
GARBAGE="$OUT/garbage.img"

# ---------------------------------------------------------------------------
# 1. Superfloppy image (BPB in sector 0, no partition table).
#    64 MiB is comfortably above the 65525-cluster FAT32 minimum.
# ---------------------------------------------------------------------------
rm -f "$IMG"
truncate -s 64M "$IMG"
mkfs.vfat -F 32 -n JOSHTEST -S 512 "$IMG" >/dev/null

# Build the file tree in a staging dir, then mcopy it in.
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

printf 'hello, josh\n' > "$STAGE/HELLO.TXT"          # 12 bytes + NUL pad
printf 'longfile'      > "$STAGE/a rather long filename.txt"
printf 'x'             > "$STAGE/this-is-a-very-long-filename-that-needs-several-lfn-entries.txt"

# A 1 MiB file with a known repeating pattern, to prove cluster-chain
# walking works across many hops and that we reassemble in the right order.
python3 - "$STAGE/BIG.BIN" <<'PY'
import sys
with open(sys.argv[1], 'wb') as f:
    f.write(bytes(i & 0xFF for i in range(1024 * 1024)))
PY

mkdir -p "$STAGE/BOOT/nested"
printf 'deep\n' > "$STAGE/BOOT/nested/deep.txt"      # 5 bytes

mcopy -i "$IMG" -s "$STAGE/HELLO.TXT" ::/
mcopy -i "$IMG" -s "$STAGE/BIG.BIN" ::/
mcopy -i "$IMG" -s "$STAGE/a rather long filename.txt" ::/
mcopy -i "$IMG" -s "$STAGE/this-is-a-very-long-filename-that-needs-several-lfn-entries.txt" ::/
mcopy -i "$IMG" -s "$STAGE/BOOT" ::/

echo "built $IMG (superfloppy)"

# ---------------------------------------------------------------------------
# 2. Partitioned image: same filesystem, offset by 2048 sectors, wrapped in
#    an MBR with a type-0x0C entry. Exercises the partition-table path.
# ---------------------------------------------------------------------------
OFFSET_SECTORS=2048
rm -f "$PART_IMG"
truncate -s $((64 * 1024 * 1024 + OFFSET_SECTORS * 512)) "$PART_IMG"
dd if="$IMG" of="$PART_IMG" bs=512 seek=$OFFSET_SECTORS conv=notrunc status=none

# Hand-build the MBR: boot signature + one partition entry at 0x1BE.
python3 - "$PART_IMG" "$OFFSET_SECTORS" <<'PY'
import struct, sys
path, off = sys.argv[1], int(sys.argv[2])
total = (64 * 1024 * 1024) // 512
with open(path, 'r+b') as f:
    mbr = bytearray(512)
    entry = struct.pack('<BBBBBBBBII',
        0x00,             # not bootable
        0xFE, 0xFF, 0xFF, # CHS start (use the "too big, see LBA" sentinel)
        0x0C,             # FAT32 LBA
        0xFE, 0xFF, 0xFF, # CHS end
        off, total)
    mbr[0x1BE:0x1BE + 16] = entry
    mbr[510] = 0x55
    mbr[511] = 0xAA
    f.seek(0)
    f.write(mbr)
PY

echo "built $PART_IMG (MBR, partition at LBA $OFFSET_SECTORS)"

# ---------------------------------------------------------------------------
# 3. Garbage image: deterministic noise with a valid 0xAA55 signature, so
#    the mounter is forced to reject it on BPB validation rather than on
#    the signature check alone.
# ---------------------------------------------------------------------------
python3 - "$GARBAGE" <<'PY'
import sys
data = bytearray((i * 37 + 11) & 0xFF for i in range(512 * 64))
data[510] = 0x55
data[511] = 0xAA
with open(sys.argv[1], 'wb') as f:
    f.write(data)
PY

echo "built $GARBAGE (invalid BPB, valid signature)"
