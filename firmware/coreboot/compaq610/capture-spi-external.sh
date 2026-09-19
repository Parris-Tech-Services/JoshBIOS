#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <probe-directory> <external-flashrom-programmer>" >&2
  echo "Example: $0 compaq610-probe ch341a_spi" >&2
  exit 2
fi

out="$1"
programmer="$2"

case "$programmer" in
  internal|internal,*)
    echo "Refusing flashrom internal programmer for Compaq 610 evidence capture." >&2
    exit 2
    ;;
esac

command -v flashrom >/dev/null 2>&1 || {
  echo "Missing required tool: flashrom" >&2
  exit 2
}
command -v sha256sum >/dev/null 2>&1 || {
  echo "Missing required tool: sha256sum" >&2
  exit 2
}

mkdir -p "$out"

flashrom -p "$programmer" --flash-name > "$out/flashrom-name.txt" 2>&1 || {
  echo "External programmer could not identify the SPI flash chip." >&2
  exit 3
}

for n in 1 2 3; do
  flashrom -p "$programmer" -r "$out/oem-rom-$n.bin"     > "$out/flashrom-read-$n.txt" 2>&1 || {
      echo "External SPI read $n failed; no write was attempted." >&2
      exit 3
    }
done

sha256sum "$out"/oem-rom-*.bin > "$out/oem-rom-sha256.txt"

first="$(sha256sum "$out/oem-rom-1.bin" | awk '{print $1}')"
second="$(sha256sum "$out/oem-rom-2.bin" | awk '{print $1}')"
third="$(sha256sum "$out/oem-rom-3.bin" | awk '{print $1}')"

if [ "$first" != "$second" ] || [ "$first" != "$third" ]; then
  echo "COMPAQ610_SPI_CAPTURE_MISMATCH" >&2
  echo "The three external reads are not byte-identical. Stop; do not write." >&2
  exit 4
fi

printf '%s\n' "$programmer" > "$out/external-programmer.txt"
echo "COMPAQ610_SPI_CAPTURE_OK"
echo "sha256=$first"
echo "No erase or write operation was performed."
