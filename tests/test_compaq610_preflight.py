#!/usr/bin/env python3
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[1]
TOOL = REPO / "firmware/coreboot/compaq610/preflight.py"
PROFILE = json.loads((REPO / "firmware/coreboot/compaq610/profile.json").read_text())

def write(path, value):
    path.write_text(value)

def run(probe):
    return subprocess.run(
        [sys.executable, str(TOOL), str(probe), "--json"],
        check=False, capture_output=True, text=True
    )

with tempfile.TemporaryDirectory() as tmp:
    probe = pathlib.Path(tmp)
    write(probe/"dmi-product_name.txt", "Compaq 610 Notebook PC\n")
    write(probe/"dmi-product_sku.txt", "VE908PA#ABG\n")
    write(probe/"dmi-board_name.txt", "308A\n")
    write(probe/"dmi-bios_version.txt", "68PVU Ver. F.20\n")
    write(probe/"lspci-nnvv.txt",
          "00:00.0 [8086:2a00]\n00:02.0 [8086:2a02]\n00:1f.0 [8086:2815]\n")
    write(probe/"flashrom-name.txt", 'Found SST flash chip "SST25VF080B" (1024 kB).\n')
    write(probe/"manual-board-silkscreen.txt", "VV09-6050A2256501-MB-A04\n")
    write(probe/"manual-spi-marking.txt", "SST25VF080B\n")

    rom = bytes((i * 37) & 0xff for i in range(PROFILE["spi_size_bytes"]))
    for n in (1,2,3):
        (probe/f"oem-rom-{n}.bin").write_bytes(rom)

    good = run(probe)
    assert good.returncode == 0, good.stdout + good.stderr
    result = json.loads(good.stdout)
    assert result["ok"]
    assert result["facts"]["oem_rom_sha256"] == hashlib.sha256(rom).hexdigest()

    (probe/"oem-rom-3.bin").write_bytes(rom[:-1] + b"X")
    bad = run(probe)
    assert bad.returncode != 0
    bad_result = json.loads(bad.stdout)
    assert not bad_result["ok"]
    assert any("byte-identical" in e for e in bad_result["errors"])

    (probe/"oem-rom-3.bin").write_bytes(rom)
    (probe/"manual-spi-marking.txt").unlink()
    bad = run(probe)
    assert bad.returncode != 0
    assert any("physical SPI marking" in e for e in json.loads(bad.stdout)["errors"])

print("Compaq 610 firmware preflight tests passed")
