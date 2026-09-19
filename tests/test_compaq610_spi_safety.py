#!/usr/bin/env python3
import pathlib

root=pathlib.Path(__file__).resolve().parents[1]
probe=(root/"firmware/coreboot/compaq610/probe-linux.sh").read_text()
external=(root/"firmware/coreboot/compaq610/capture-spi-external.sh").read_text()

assert "flashrom -p internal" not in probe
assert "does not access the SPI flash" in probe
assert 'internal|internal,*' in external
assert 'flashrom -p "$programmer" -r' in external
assert "COMPAQ610_SPI_CAPTURE_MISMATCH" in external
assert "No erase or write operation was performed." in external
assert " -w " not in external
assert " --write" not in external
assert " -E " not in external
assert " --erase" not in external

print("Compaq 610 external SPI capture safety tests passed")
