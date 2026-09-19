#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${COREBOOT_WORK:-$repo_root/build/coreboot-src}"
revision="${COREBOOT_REV:-cb2619b3bdd60e56bb1ff970f9e2126c3042668e}"

rm -rf "$work"
git clone --filter=blob:none https://github.com/coreboot/coreboot.git "$work"
git -C "$work" checkout --detach "$revision"

host_make=(HOSTCC=clang HOSTCXX=clang++)
make -C "$work" crossgcc-i386 CPUS="$(nproc)"
make -C "$work" "${host_make[@]}" distclean
make -C "$work" "${host_make[@]}" defconfig KBUILD_DEFCONFIG="$repo_root/firmware/coreboot/qemu-defconfig"
make -C "$work" "${host_make[@]}" olddefconfig
make -C "$work" "${host_make[@]}" -j"$(nproc)"

test -s "$work/build/coreboot.rom"
cp "$work/build/coreboot.rom" "$repo_root/build/coreboot-qemu.rom"
sha256sum "$repo_root/build/coreboot-qemu.rom" > "$repo_root/build/coreboot-qemu.rom.sha256"

echo "coreboot QEMU firmware built at build/coreboot-qemu.rom"
