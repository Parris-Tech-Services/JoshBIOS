#!/usr/bin/env bash
set -euo pipefail

coreboot="${1:-build/coreboot-src}"
profile_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
profile="$profile_dir/profile.json"

if [ ! -d "$coreboot/.git" ]; then
  echo "coreboot source tree not found: $coreboot" >&2
  exit 2
fi

python3 - "$coreboot" "$profile" <<'PY'
import json, pathlib, sys
root=pathlib.Path(sys.argv[1])
profile=json.loads(pathlib.Path(sys.argv[2]).read_text())
missing=[path for path in profile["coreboot_platform_requirements"]
         if not (root/path).is_file()]
if missing:
    print("COMPAQ610_COREBOOT_PLATFORM_BLOCKED")
    for path in missing:
        print(f"- missing upstream prerequisite: {path}")
    raise SystemExit(1)
print("COMPAQ610_COREBOOT_PLATFORM_PREREQS_OK")
print("GM965/ICH8M support is present; board-specific Compaq 610 code is still required.")
PY
