#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent
DEFAULT_PROFILE = ROOT / "profile.json"

def read_text(path):
    try:
        return path.read_text(errors="replace").strip()
    except FileNotFoundError:
        return ""

def norm(value):
    return " ".join(value.strip().split()).lower()

def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def validate(probe_dir, profile):
    errors = []
    facts = {}

    product = read_text(probe_dir / "dmi-product_name.txt")
    sku = read_text(probe_dir / "dmi-product_sku.txt")
    board = read_text(probe_dir / "dmi-board_name.txt")
    bios = read_text(probe_dir / "dmi-bios_version.txt")
    lspci = read_text(probe_dir / "lspci-nnvv.txt")
    flash_name = read_text(probe_dir / "flashrom-name.txt")
    manual_board = read_text(probe_dir / "manual-board-silkscreen.txt")
    manual_spi = read_text(probe_dir / "manual-spi-marking.txt")
    capture_programmer = read_text(probe_dir / "external-programmer.txt")

    facts.update(product=product, sku=sku, baseboard=board, bios=bios,
                 manual_board=manual_board, manual_spi=manual_spi,
                 capture_programmer=capture_programmer)

    if norm(profile["system_product"]) not in norm(product):
        errors.append(f"product mismatch: expected {profile['system_product']!r}, got {product!r}")
    if norm(sku) != norm(profile["system_sku"]):
        errors.append(f"SKU mismatch: expected {profile['system_sku']!r}, got {sku!r}")
    if norm(board) != norm(profile["baseboard_product"]):
        errors.append(f"baseboard mismatch: expected {profile['baseboard_product']!r}, got {board!r}")
    if norm(profile["bios_family"]) not in norm(bios):
        errors.append(f"BIOS family mismatch: expected {profile['bios_family']!r} in {bios!r}")

    pci_lower = lspci.lower()
    for pci_id in profile["required_pci_ids"]:
        if pci_id.lower() not in pci_lower:
            errors.append(f"required PCI ID missing: {pci_id}")

    chip_token = profile["spi_chip"].lower()
    compact_flash = re.sub(r"[^a-z0-9]", "", flash_name.lower())
    compact_chip = re.sub(r"[^a-z0-9]", "", chip_token)
    if compact_chip not in compact_flash:
        errors.append(f"SPI chip mismatch: expected {profile['spi_chip']} in flashrom output")

    if norm(manual_board) != norm(profile["board_silkscreen"]):
        errors.append(
            "physical motherboard silkscreen not confirmed: "
            f"expected {profile['board_silkscreen']!r}"
        )
    if compact_chip not in re.sub(r"[^a-z0-9]", "", manual_spi.lower()):
        errors.append(
            "physical SPI marking not confirmed: "
            f"expected {profile['spi_chip']!r}"
        )

    if not capture_programmer:
        errors.append("external SPI capture programmer not recorded")
    elif norm(capture_programmer).startswith("internal"):
        errors.append("internal flashrom programmer is not accepted as OEM dump evidence")

    roms = [probe_dir / f"oem-rom-{n}.bin" for n in (1, 2, 3)]
    hashes = []
    for rom in roms:
        if not rom.is_file():
            errors.append(f"missing independent OEM ROM read: {rom.name}")
            continue
        size = rom.stat().st_size
        if size != profile["spi_size_bytes"]:
            errors.append(
                f"{rom.name} size {size} != expected {profile['spi_size_bytes']}"
            )
        hashes.append(sha256(rom))

    if len(hashes) == 3 and len(set(hashes)) != 1:
        errors.append("three OEM ROM reads are not byte-identical")

    facts["oem_rom_sha256"] = hashes[0] if len(hashes) == 3 and len(set(hashes)) == 1 else ""
    return errors, facts

def main():
    parser = argparse.ArgumentParser(
        description="Fail-closed Compaq 610 hardware identity preflight"
    )
    parser.add_argument("probe_dir", type=pathlib.Path)
    parser.add_argument("--profile", type=pathlib.Path, default=DEFAULT_PROFILE)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    profile = json.loads(args.profile.read_text())
    errors, facts = validate(args.probe_dir, profile)
    result = {
        "ok": not errors,
        "canonical_board_id": profile["canonical_board_id"],
        "profile_status": profile["status"],
        "facts": facts,
        "errors": errors,
    }
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    elif errors:
        print("COMPAQ610_PREFLIGHT_BLOCKED")
        for error in errors:
            print(f"- {error}")
    else:
        print("COMPAQ610_PREFLIGHT_OK")
        print(f"board_id={profile['canonical_board_id']}")
        print(f"oem_rom_sha256={facts['oem_rom_sha256']}")

    return 0 if not errors else 1

if __name__ == "__main__":
    sys.exit(main())
