#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${COREBOOT_WORK:-$repo_root/build/coreboot-src}"
revision="${COREBOOT_REV:-cb2619b3bdd60e56bb1ff970f9e2126c3042668e}"

rm -rf "$work"
git clone --filter=blob:none https://github.com/coreboot/coreboot.git "$work"
git -C "$work" checkout --detach "$revision"

# GitHub-hosted runners have intermittently failed on ftpmirror.gnu.org's
# large GCC archive. Use coreboot's own source mirror; buildgcc still verifies
# every cached archive against the pinned upstream checksum before unpacking.
sed -i 's/^USE_COREBOOT_MIRROR=0$/USE_COREBOOT_MIRROR=1/' "$work/util/crossgcc/buildgcc"

host_make=(HOSTCC=clang HOSTCXX=clang++)
for attempt in 1 2 3; do
    if make -C "$work" crossgcc-i386 CPUS="$(nproc)"; then
        break
    fi
    if (( attempt == 3 )); then
        echo "coreboot cross-toolchain build failed after $attempt attempts" >&2
        exit 1
    fi
    echo "coreboot cross-toolchain build failed; retrying transient download/build failure ($attempt/3)" >&2
    sleep $((attempt * 10))
done
make -C "$work" "${host_make[@]}" distclean
make -C "$work" "${host_make[@]}" defconfig KBUILD_DEFCONFIG="$repo_root/firmware/coreboot/qemu-defconfig"
make -C "$work" "${host_make[@]}" olddefconfig
make -C "$work" "${host_make[@]}" -j"$(nproc)"

test -s "$work/build/coreboot.rom"
test -x "$work/build/cbfstool"

# SeaBIOS is a coreboot payload here, so make the smoke-test boot path explicit
# in CBFS instead of relying on an interactive menu/default-device timeout.
# The path below is the QEMU i440fx primary ATA disk reported by SeaBIOS.
runtime_cfg="$repo_root/build/coreboot-runtime"
mkdir -p "$runtime_cfg"
printf '%s\n' '/pci@i0cf8/*@1,1/drive@0/disk@0' > "$runtime_cfg/bootorder"
printf '\x00\x00\x00\x00' > "$runtime_cfg/show-boot-menu"

"$work/build/cbfstool" "$work/build/coreboot.rom" add \
    -f "$runtime_cfg/bootorder" -n bootorder -t raw
"$work/build/cbfstool" "$work/build/coreboot.rom" add \
    -f "$runtime_cfg/show-boot-menu" -n etc/show-boot-menu -t raw

"$work/build/cbfstool" "$work/build/coreboot.rom" print | grep -q 'bootorder'
"$work/build/cbfstool" "$work/build/coreboot.rom" print | grep -q 'etc/show-boot-menu'

cp "$work/build/coreboot.rom" "$repo_root/build/coreboot-qemu.rom"
sha256sum "$repo_root/build/coreboot-qemu.rom" > "$repo_root/build/coreboot-qemu.rom.sha256"

echo "coreboot QEMU firmware built at build/coreboot-qemu.rom"
