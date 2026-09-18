CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
HOST_CC ?= cc
UEFI_CC ?= clang
UEFI_LD ?= lld-link
BUILD := build
STAGE2_SECTORS := 16
KERNEL_SECTORS := 64

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -Wall -Wextra -Werror -O2
ASFLAGS := -m32 -ffreestanding -fno-pie
HOST_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -Iboot/core
UEFI_CFLAGS := --target=x86_64-pc-win32-coff -std=c11 -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -Wall -Wextra -Werror -O2

OVMF_CODE ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd))
OVMF_VARS ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_VARS_4M.fd /usr/share/OVMF/OVMF_VARS.fd))

.PHONY: all clean image check run smoke test uefi uefi-image uefi-smoke
all: image check

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/stage1.o: boot/stage1.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/stage1.bin: $(BUILD)/stage1.o
	$(LD) -m elf_i386 -Ttext 0x7C00 --oformat binary -e _start $< -o $@

$(BUILD)/stage2.o: boot/stage2.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/stage2.bin: $(BUILD)/stage2.o
	$(LD) -m elf_i386 -Ttext 0x8000 --oformat binary -e _start $< -o $@

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
	@stage2_size=$$(wc -c < $(BUILD)/stage2.bin); max=$$(( $(STAGE2_SECTORS) * 512 )); \
	  test $$stage2_size -le $$max || { echo "stage2 too large: $$stage2_size > $$max"; exit 1; }
	@kernel_size=$$(wc -c < $(BUILD)/kernel.bin); max=$$(( $(KERNEL_SECTORS) * 512 )); \
	  test $$kernel_size -le $$max || { echo "kernel too large: $$kernel_size > $$max"; exit 1; }
	truncate -s 1048576 $(BUILD)/joshbios.img
	dd if=$(BUILD)/stage1.bin of=$(BUILD)/joshbios.img conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$(BUILD)/joshbios.img bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/joshbios.img bs=512 seek=17 conv=notrunc status=none

check: image
	@test $$(wc -c < $(BUILD)/stage1.bin) -eq 512
	@sig=$$(od -An -tx1 -j510 -N2 $(BUILD)/stage1.bin | tr -d ' \n'); test "$$sig" = "55aa"
	@echo "JoshBIOS image OK: $(BUILD)/joshbios.img"

$(BUILD)/test_boot_storage: boot/core/partition.c boot/core/partition.h boot/core/fat32.c boot/core/fat32.h boot/core/block.h tests/test_boot_storage.c | $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) boot/core/partition.c boot/core/fat32.c tests/test_boot_storage.c -o $@

test: $(BUILD)/test_boot_storage
	$(BUILD)/test_boot_storage

run: all
	qemu-system-i386 -drive format=raw,file=$(BUILD)/joshbios.img

smoke: all
	@rm -f $(BUILD)/boot.log; \
	  status=0; \
	  timeout 10s qemu-system-i386 \
	    -drive format=raw,file=$(BUILD)/joshbios.img \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/boot.log 2>&1 || status=$$?; \
	  test $$status -eq 0 -o $$status -eq 124; \
	  grep -q JOSHBIOS_BOOTINFO_OK $(BUILD)/boot.log; \
	  grep -q JOSHBIOS_BOOT_OK $(BUILD)/boot.log; \
	  echo "JoshBIOS QEMU boot smoke test passed."

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
	@rm -f $(BUILD)/uefi.log; \
	  status=0; \
	  timeout 15s qemu-system-x86_64 \
	    -machine q35 \
	    -drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
	    -drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
	    -drive format=raw,file=$(BUILD)/joshuefi.img \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/uefi.log 2>&1 || status=$$?; \
	  test $$status -eq 0 -o $$status -eq 124; \
	  grep -q JOSHUEFI_ENTRY_OK $(BUILD)/uefi.log; \
	  echo "JoshUEFI OVMF boot smoke test passed."

clean:
	rm -rf $(BUILD)
