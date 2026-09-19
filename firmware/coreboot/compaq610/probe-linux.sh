#!/usr/bin/env bash
set -euo pipefail

out="${1:-compaq610-probe}"
mkdir -p "$out"

if [ "${EUID:-$(id -u)}" -ne 0 ]; then
  echo "This collector needs root for read-only firmware/chipset inspection." >&2
  exit 2
fi

required=(dmidecode lspci)
for command_name in "${required[@]}"; do
  command -v "$command_name" >/dev/null 2>&1 || {
    echo "Missing required tool: $command_name" >&2
    exit 2
  }
done

copy_dmi() {
  local name="$1"
  if [ -r "/sys/class/dmi/id/$name" ]; then
    tr -d '\000' < "/sys/class/dmi/id/$name" > "$out/dmi-$name.txt"
  else
    : > "$out/dmi-$name.txt"
  fi
}

for field in sys_vendor product_name product_version product_sku              board_vendor board_name board_version bios_vendor bios_version bios_date; do
  copy_dmi "$field"
done

dmidecode --type bios --type system --type baseboard > "$out/dmidecode.txt"
lspci -nnvv > "$out/lspci-nnvv.txt"
lspci -xxxx > "$out/lspci-config-space.txt"

# These utilities only inspect platform state. Absence is recorded rather than
# making the capture unusable.
if command -v inteltool >/dev/null 2>&1; then
  inteltool -a > "$out/inteltool.txt" 2>&1 || true
else
  echo "inteltool unavailable" > "$out/inteltool.txt"
fi
if command -v superiotool >/dev/null 2>&1; then
  superiotool -ade > "$out/superiotool.txt" 2>&1 || true
else
  echo "superiotool unavailable" > "$out/superiotool.txt"
fi

cat > "$out/MANUAL-EVIDENCE.txt" <<'EOF'
LIVE HARDWARE CAPTURE COMPLETE.

This laptop-side collector deliberately does not access the SPI flash through
flashrom's internal programmer.

Before any physical firmware image can be considered flash-ready:
1. Open the laptop and record the exact motherboard silkscreen in:
   manual-board-silkscreen.txt
2. Record the exact marking printed on U23 in:
   manual-spi-marking.txt
3. Power the target down and use capture-spi-external.sh from the programmer
   host to obtain three independent OEM ROM reads.
4. Externally restore the untouched OEM image with the intended programmer,
   boot the laptop successfully, then create recovery-proof.json using the
   schema in firmware/coreboot/compaq610/recovery-proof.example.json.

The collector has not read, erased or written the SPI flash.
EOF

echo "Compaq 610 live hardware evidence captured in: $out"
