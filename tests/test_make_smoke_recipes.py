#!/usr/bin/env python3
import pathlib

makefile=(pathlib.Path(__file__).resolve().parents[1]/"Makefile").read_text()
start=makefile.index("bridge-config-fail-smoke:")
end=makefile.index("$(BUILD)/uefi_main.obj:",start)
block=makefile[start:end]

status_scrubbed=block.replace("$$status","")
log_scrubbed=block.replace("$$log","")
assert "$status" not in status_scrubbed, "single-dollar $status in Make recipe"
assert "$log" not in log_scrubbed, "single-dollar $log in Make recipe"
assert "status=$$?" in block
assert block.count("test $$status -eq 0 -o $$status -eq 124") >= 4

print("Make smoke-recipe escaping tests passed")
