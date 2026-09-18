#!/usr/bin/env python3
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(
        description="Fail-closed proof gate before any Compaq 610 flash backend work"
    )
    parser.add_argument("probe_dir", type=pathlib.Path)
    parser.add_argument("recovery_proof", type=pathlib.Path)
    args = parser.parse_args()

    preflight = subprocess.run(
        [sys.executable, str(ROOT / "preflight.py"), str(args.probe_dir), "--json"],
        check=False, capture_output=True, text=True
    )
    if preflight.returncode != 0:
        print("COMPAQ610_FLASH_GATE_BLOCKED")
        print(preflight.stdout.strip())
        return 1

    evidence = json.loads(preflight.stdout)
    proof = json.loads(args.recovery_proof.read_text())
    facts = evidence["facts"]

    errors = []
    if proof.get("proof_version") != 1:
        errors.append("unsupported recovery proof version")
    if not proof.get("programmer_model") or "REPLACE-" in proof.get("programmer_model", ""):
        errors.append("exact external programmer not recorded")
    if proof.get("programmer_voltage") != "3.3V":
        errors.append("programmer voltage must be explicitly recorded as 3.3V")
    if proof.get("spi_chip", "").lower() != facts["manual_spi"].lower():
        errors.append("recovery proof SPI chip does not match physical marking")
    if proof.get("board_silkscreen", "").lower() != facts["manual_board"].lower():
        errors.append("recovery proof board does not match physical silkscreen")
    if proof.get("oem_rom_sha256", "").lower() != facts["oem_rom_sha256"].lower():
        errors.append("recovery proof OEM ROM hash does not match captured OEM ROM")
    for field in (
        "external_write_completed",
        "external_readback_sha256_matches",
        "oem_boot_after_restore_verified",
    ):
        if proof.get(field) is not True:
            errors.append(f"{field} is not proven")

    if errors:
        print("COMPAQ610_FLASH_GATE_BLOCKED")
        for error in errors:
            print(f"- {error}")
        return 1

    print("COMPAQ610_FLASH_GATE_OPEN")
    print("This proves recovery readiness only; it does not prove a coreboot port works.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
