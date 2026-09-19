#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
PROFILE=json.loads((ROOT/"firmware/coreboot/compaq610/profile.json").read_text())
TOOL=ROOT/"firmware/coreboot/compaq610/make-port-input.py"

with tempfile.TemporaryDirectory() as tmp:
    d=pathlib.Path(tmp)
    values={
        "dmi-product_name.txt":"Compaq 610 Notebook PC\n",
        "dmi-product_sku.txt":"VE908PA#ABG\n",
        "dmi-board_name.txt":"308A\n",
        "dmi-bios_version.txt":"68PVU Ver. F.20\n",
        "lspci-nnvv.txt":"00:00.0 [8086:2a00]\n00:02.0 [8086:2a02]\n00:1f.0 [8086:2815]\n",
        "lspci-config-space.txt":"00:00.0 config fixture\n",
        "dmidecode.txt":"Compaq 610 fixture\n",
        "inteltool.txt":"GPIOBASE 0x0480\nGPIO_USE_SEL=0x12345678\n",
        "superiotool.txt":"Found SMSC fixture\n",
        "flashrom-name.txt":'Found SST flash chip "SST25VF080B" (1024 kB).\n',
        "manual-board-silkscreen.txt":"VV09-6050A2256501-MB-A04\n",
        "manual-spi-marking.txt":"SST25VF080B\n",
        "external-programmer.txt":"ch341a_spi\n",
    }
    for name,value in values.items():
        (d/name).write_text(value)
    rom=bytes((i*17)&0xff for i in range(PROFILE["spi_size_bytes"]))
    for n in (1,2,3):
        (d/f"oem-rom-{n}.bin").write_bytes(rom)

    out=d/"port-input.json"
    result=subprocess.run([sys.executable,str(TOOL),str(d),str(out)],
                          text=True,capture_output=True)
    assert result.returncode==0,result.stdout+result.stderr
    data=json.loads(out.read_text())
    assert data["board_id"]=="hp-compaq-610-vv09-a04-uma"
    assert data["port_status"]=="captured-not-implemented"
    assert data["capture_files"]["oem-rom-1.bin"]["sha256"] == data["capture_files"]["oem-rom-3.bin"]["sha256"]

    (d/"inteltool.txt").write_text("inteltool unavailable\n")
    blocked=subprocess.run([sys.executable,str(TOOL),str(d),str(out)],
                           text=True,capture_output=True)
    assert blocked.returncode!=0
    assert "inteltool capture is required" in blocked.stdout

print("Compaq 610 port-input manifest tests passed")
