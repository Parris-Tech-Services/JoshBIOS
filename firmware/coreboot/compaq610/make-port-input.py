#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent
PREFLIGHT = ROOT / "preflight.py"

REQUIRED_CAPTURE_FILES = (
    "dmidecode.txt",
    "lspci-nnvv.txt",
    "lspci-config-space.txt",
    "inteltool.txt",
    "superiotool.txt",
    "flashrom-name.txt",
    "manual-board-silkscreen.txt",
    "manual-spi-marking.txt",
    "external-programmer.txt",
    "oem-rom-1.bin",
    "oem-rom-2.bin",
    "oem-rom-3.bin",
)

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def main():
    parser=argparse.ArgumentParser(
        description="Freeze a verified Compaq 610 capture into reproducible coreboot port inputs"
    )
    parser.add_argument("probe_dir", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args=parser.parse_args()

    check=subprocess.run(
        [sys.executable,str(PREFLIGHT),str(args.probe_dir),"--json"],
        text=True,capture_output=True,check=False
    )
    if check.returncode != 0:
        print(check.stdout,end="")
        return 1
    preflight=json.loads(check.stdout)

    missing=[name for name in REQUIRED_CAPTURE_FILES
             if not (args.probe_dir/name).is_file()]
    if missing:
        print("COMPAQ610_PORT_INPUT_BLOCKED")
        for name in missing:
            print(f"- missing capture file: {name}")
        return 1

    intel=(args.probe_dir/"inteltool.txt").read_text(errors="replace")
    sio=(args.probe_dir/"superiotool.txt").read_text(errors="replace")
    if "inteltool unavailable" in intel:
        print("COMPAQ610_PORT_INPUT_BLOCKED\n- inteltool capture is required")
        return 1
    if "superiotool unavailable" in sio:
        print("COMPAQ610_PORT_INPUT_BLOCKED\n- superiotool capture is required")
        return 1

    files={}
    for name in REQUIRED_CAPTURE_FILES:
        path=args.probe_dir/name
        files[name]={"bytes":path.stat().st_size,"sha256":digest(path)}

    manifest={
        "format_version":1,
        "board_id":preflight["canonical_board_id"],
        "preflight_facts":preflight["facts"],
        "capture_files":files,
        "port_status":"captured-not-implemented",
        "required_next_evidence":[
            "derive GPIO directions/levels/inversion from inteltool capture",
            "derive Super I/O and EC decode from superiotool/vendor firmware evidence",
            "confirm SPD SMBus addresses on both installed DIMM slots",
            "build a board-specific coreboot ROM without flashing it",
            "prove OEM recovery with the external programmer before first flash"
        ]
    }
    args.output.write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n")
    print(f"COMPAQ610_PORT_INPUT_OK {args.output}")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
