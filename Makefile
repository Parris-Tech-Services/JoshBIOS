CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
HOSTCC ?= cc
UEFI_CC ?= clang
UEFI_LD ?= lld-link
BUILD := build
STAGE2_SECTORS := 32
KERNEL_SECTORS := 512
ASHFALLEN_KERNEL ?= ashfallen/kernel/bin/kernel
BRIDGE_IMAGE := $(BUILD)/joshbios-ashfallen.img
FAT32_BRIDGE_IMAGE := $(BUILD)/joshbios-ashfallen-fat32.img

CFLAGS := -m32 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -Wall -Wextra -Werror -O2
ASFLAGS := -m32 -ffreestanding -fno-pie
HOST_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror
UEFI_CFLAGS := --target=x86_64-pc-win32-coff -std=c11 -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -Wall -Wextra -Werror -O2
BOOT_CPPFLAGS := -Iboot -Iboot/core

OVMF_CODE ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd))
OVMF_VARS ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_VARS_4M.fd /usr/share/OVMF/OVMF_VARS.fd))

.PHONY: all clean image check run smoke host-tests bridge-image bridge-smoke fat32-bridge-image fat32-bridge-smoke uefi uefi-image uefi-smoke
all: image check

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_elf64: tests/test_elf64.c boot/elf64.c boot/elf64.h | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Iboot tests/test_elf64.c boot/elf64.c -o $@

$(BUILD)/test_boot_storage: tests/test_boot_storage.c boot/core/block.h boot/core/partition.h boot/core/partition.c boot/core/fat32.h boot/core/fat32.c | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Iboot/core tests/test_boot_storage.c boot/core/partition.c boot/core/fat32.c -o $@

host-tests: $(BUILD)/test_elf64 $(BUILD)/test_boot_storage
	$(BUILD)/test_elf64
	$(BUILD)/test_boot_storage

$(BUILD)/stage1.o: boot/stage1.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/stage1.bin: $(BUILD)/stage1.o
	$(LD) -m elf_i386 -Ttext 0x7C00 --oformat binary -e _start $< -o $@

$(BUILD)/stage2.o: boot/stage2.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/bios_block.o: boot/bios_block.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/stage2_pm.o: boot/stage2_pm.c boot/elf64.h boot/protocol.h boot/core/block.h boot/core/partition.h boot/core/fat32.h | $(BUILD)
	$(CC) $(CFLAGS) $(BOOT_CPPFLAGS) -c $< -o $@

$(BUILD)/elf64_pm.o: boot/elf64.c boot/elf64.h | $(BUILD)
	$(CC) $(CFLAGS) $(BOOT_CPPFLAGS) -c $< -o $@

$(BUILD)/partition_pm.o: boot/core/partition.c boot/core/partition.h boot/core/block.h | $(BUILD)
	$(CC) $(CFLAGS) $(BOOT_CPPFLAGS) -c $< -o $@

$(BUILD)/fat32_pm.o: boot/core/fat32.c boot/core/fat32.h boot/core/block.h boot/core/partition.h | $(BUILD)
	$(CC) $(CFLAGS) $(BOOT_CPPFLAGS) -c $< -o $@

$(BUILD)/runtime32.o: boot/runtime32.c | $(BUILD)
	$(CC) $(CFLAGS) $(BOOT_CPPFLAGS) -c $< -o $@

STAGE2_OBJECTS := 	$(BUILD)/stage2.o 	$(BUILD)/bios_block.o 	$(BUILD)/stage2_pm.o 	$(BUILD)/elf64_pm.o 	$(BUILD)/partition_pm.o 	$(BUILD)/fat32_pm.o 	$(BUILD)/runtime32.o

$(BUILD)/stage2.bin: $(STAGE2_OBJECTS)
	$(LD) -m elf_i386 -Ttext 0x8000 --oformat binary -e _start $^ -o $@

$(BUILD)/entry.o: kernel/entry.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel.o: kernel/kernel.c kernel/bootinfo.h kernel/serial.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/serial.o: kernel/serial.c kernel/bootinfo.h kernel/serial.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/serial.o kernel/linker.ld
	$(LD) -m elf_i386 -T kernel/linker.ld -nostdlib $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/serial.o -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

image: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	@stage2_size=$$(wc -c < $(BUILD)/stage2.bin); max=$$(( $(STAGE2_SECTORS) * 512 )); 	  test $$stage2_size -le $$max || { echo "stage2 too large: $$stage2_size > $$max"; exit 1; }
	@kernel_size=$$(wc -c < $(BUILD)/kernel.bin); max=$$(( $(KERNEL_SECTORS) * 512 )); 	  test $$kernel_size -le $$max || { echo "kernel too large: $$kernel_size > $$max"; exit 1; }
	truncate -s 1048576 $(BUILD)/joshbios.img
	dd if=$(BUILD)/stage1.bin of=$(BUILD)/joshbios.img conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$(BUILD)/joshbios.img bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/joshbios.img bs=512 seek=33 conv=notrunc status=none

check: image
	@test $$(wc -c < $(BUILD)/stage1.bin) -eq 512
	@sig=$$(od -An -tx1 -j510 -N2 $(BUILD)/stage1.bin | tr -d ' \n'); test "$$sig" = "55aa"
	@echo "JoshBIOS image OK: $(BUILD)/joshbios.img"

run: all
	qemu-system-i386 -drive format=raw,file=$(BUILD)/joshbios.img

smoke: all
	@rm -f $(BUILD)/boot.log; 	  status=0; 	  timeout 10s qemu-system-i386 	    -drive format=raw,file=$(BUILD)/joshbios.img 	    -display none -serial stdio -monitor none -no-reboot 	    > $(BUILD)/boot.log 2>&1 || status=$$?; 	  test $$status -eq 0 -o $$status -eq 124; 	  grep -q JOSHBIOS_BOOTINFO_OK $(BUILD)/boot.log; 	  grep -q JOSHBIOS_BOOT_OK $(BUILD)/boot.log; 	  echo "JoshBIOS QEMU boot smoke test passed."

bridge-image: $(BUILD)/stage1.bin $(BUILD)/stage2.bin
	@test -f "$(ASHFALLEN_KERNEL)" || { echo "AshFallen kernel not found: $(ASHFALLEN_KERNEL)"; exit 1; }
	@kernel_size=$$(wc -c < "$(ASHFALLEN_KERNEL)"); max=$$(( $(KERNEL_SECTORS) * 512 )); 	  test $$kernel_size -le $$max || { echo "AshFallen kernel too large for bootstrap extent: $$kernel_size > $$max"; exit 1; }
	truncate -s 2097152 $(BRIDGE_IMAGE)
	dd if=$(BUILD)/stage1.bin of=$(BRIDGE_IMAGE) conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$(BRIDGE_IMAGE) bs=512 seek=1 conv=notrunc status=none
	dd if="$(ASHFALLEN_KERNEL)" of=$(BRIDGE_IMAGE) bs=512 seek=33 conv=notrunc status=none
	@echo "JoshBootloader + AshFallen bridge image ready: $(BRIDGE_IMAGE)"

bridge-smoke: bridge-image
	@rm -f $(BUILD)/bridge.log; 	  status=0; 	  timeout 15s qemu-system-x86_64 	    -machine pc -m 256M 	    -drive format=raw,file=$(BRIDGE_IMAGE) 	    -display none -serial stdio -monitor none -no-reboot 	    > $(BUILD)/bridge.log 2>&1 || status=$$?; 	  test $$status -eq 0 -o $$status -eq 124; 	  grep -q JOSHBOOT_ELF64_DETECTED $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_HANDOFF_READY $(BUILD)/bridge.log; 	  grep -q JOSHOS_KERNEL_ENTERED $(BUILD)/bridge.log; 	  grep -q JOSHOS_BOOT_ADAPTER_OK $(BUILD)/bridge.log; 	  grep -q JOSHOS_BOOT_OK $(BUILD)/bridge.log; 	  echo "JoshBootloader -> AshFallen QEMU bridge smoke test passed."

fat32-bridge-image: $(BUILD)/stage1.bin $(BUILD)/stage2.bin
	@test -f "$(ASHFALLEN_KERNEL)" || { echo "AshFallen kernel not found: $(ASHFALLEN_KERNEL)"; exit 1; }
	bash scripts/build-fat32-bridge.sh $(FAT32_BRIDGE_IMAGE) $(BUILD)/stage1.bin $(BUILD)/stage2.bin "$(ASHFALLEN_KERNEL)"

fat32-bridge-smoke: fat32-bridge-image
	@rm -f $(BUILD)/fat32-bridge.log; \
	  status=0; \
	  timeout 20s qemu-system-x86_64 \
	    -machine pc -m 256M \
	    -drive format=raw,file=$(FAT32_BRIDGE_IMAGE) \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/fat32-bridge.log 2>&1 || status=$?; \
	  test $status -eq 0 -o $status -eq 124; \
	  grep -q JOSHBOOT_FAT32_KERNEL_OK $(BUILD)/fat32-bridge.log; \
	  ! grep -q JOSHBOOT_NO_ELF64_USING_LEGACY_FALLBACK $(BUILD)/fat32-bridge.log; \
	  grep -q JOSHBOOT_ELF64_DETECTED $(BUILD)/fat32-bridge.log; \
	  grep -q JOSHBOOT_HANDOFF_READY $(BUILD)/fat32-bridge.log; \
	  grep -q JOSHOS_KERNEL_ENTERED $(BUILD)/fat32-bridge.log; \
	  grep -q JOSHOS_BOOT_ADAPTER_OK $(BUILD)/fat32-bridge.log; \
	  grep -q JOSHOS_BOOT_OK $(BUILD)/fat32-bridge.log; \
	  echo "FAT32 -> JoshBootloader -> AshFallen QEMU smoke test passed."

$(BUILD)/uefi_main.obj: boot/uefi/main.c boot/uefi/efi.h | $(BUILD)
	$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD)/BOOTX64.EFI: $(BUILD)/uefi_main.obj
	$(UEFI_LD) /subsystem:efi_application /entry:efi_main /nodefaultlib /out:$@ $<

uefi: $(BUILD)/BOOTX64.EFI
	@file $(BUILD)/BOOTX64.EFI | grep -q 'for EFI (application)'
	@echo "JoshUEFI application OK: $(BUILD)/BOOTX64.EFI"

$(BUILD)/joshuefi.img: $(BUILD)/BOOTX64.EFI
	truncate -s 67108864 $@
	mformat -i $@ -F ::
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $(BUILD)/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

uefi-image: $(BUILD)/joshuefi.img
	@echo "JoshUEFI removable-media image OK: $(BUILD)/joshuefi.img"

uefi-smoke: uefi-image
	@test -n "$(OVMF_CODE)" || { echo "OVMF code image not found"; exit 1; }
	@test -n "$(OVMF_VARS)" || { echo "OVMF vars image not found"; exit 1; }
	@cp "$(OVMF_VARS)" $(BUILD)/OVMF_VARS.fd
	@rm -f $(BUILD)/uefi.log; 	  status=0; 	  timeout 15s qemu-system-x86_64 	    -machine q35 	    -drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" 	    -drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd 	    -drive format=raw,file=$(BUILD)/joshuefi.img 	    -display none -serial stdio -monitor none -no-reboot 	    > $(BUILD)/uefi.log 2>&1 || status=$$?; 	  test $$status -eq 0 -o $$status -eq 124; 	  grep -q JOSHUEFI_ENTRY_OK $(BUILD)/uefi.log; 	  echo "JoshUEFI OVMF boot smoke test passed."

clean:
	rm -rf $(BUILD)
