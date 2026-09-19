CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
HOSTCC ?= cc
PYTHON ?= python3
UEFI_CC ?= clang
UEFI_LD ?= lld-link
BUILD := build
STAGE2_SECTORS := 64
KERNEL_SECTORS := 8192
ASHFALLEN_KERNEL ?= ashfallen/kernel/bin/kernel
BRIDGE_IMAGE := $(BUILD)/joshbios-ashfallen.img
BAD_CONFIG_IMAGE := $(BUILD)/joshbios-bad-config.img
ROLLBACK_IMAGE := $(BUILD)/joshbios-rollback.img
RECOVERY_IMAGE := $(BUILD)/joshbios-recovery.img
UEFI_RECOVERY_IMAGE := $(BUILD)/joshuefi-recovery.img

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -Wall -Wextra -Werror -O2
ASFLAGS := -m32 -ffreestanding -fno-pie
HOST_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror
UEFI_CFLAGS := --target=x86_64-pc-win32-coff -std=c11 -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -fno-builtin -Wall -Wextra -Werror -O2 -Iboot -Iboot/core

OVMF_CODE ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd))
OVMF_VARS ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_VARS_4M.fd /usr/share/OVMF/OVMF_VARS.fd))

.PHONY: all clean image check run smoke host-tests bridge-image bridge-smoke bridge-config-fail-smoke bridge-rollback-smoke bridge-recovery-smoke uefi uefi-image uefi-smoke uefi-recovery-smoke
all: image check

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_elf64: tests/test_elf64.c boot/elf64.c boot/elf64.h | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Iboot tests/test_elf64.c boot/elf64.c -o $@

$(BUILD)/test_boot_storage: tests/test_boot_storage.c boot/core/block.h boot/core/partition.h boot/core/partition.c boot/core/fat32.h boot/core/fat32.c | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Iboot/core tests/test_boot_storage.c boot/core/partition.c boot/core/fat32.c -o $@

$(BUILD)/test_boot_config: tests/test_boot_config.c boot/core/config.h boot/core/config.c | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Iboot/core tests/test_boot_config.c boot/core/config.c -o $@

$(BUILD)/test_boot_health: tests/test_boot_health.c boot/core/health.h boot/core/health.c | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Iboot/core tests/test_boot_health.c boot/core/health.c -o $@

$(BUILD)/test_firmware_update: tests/test_firmware_update.c firmware/update.h firmware/update.c | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Ifirmware tests/test_firmware_update.c firmware/update.c -o $@

$(BUILD)/josh-healthctl: tools/josh-healthctl.c boot/core/health.h boot/core/health.c | $(BUILD)
	$(HOSTCC) $(HOST_CFLAGS) -Iboot/core tools/josh-healthctl.c boot/core/health.c -o $@

host-tests: $(BUILD)/test_elf64 $(BUILD)/test_boot_storage $(BUILD)/test_boot_config $(BUILD)/test_boot_health $(BUILD)/test_firmware_update
	$(BUILD)/test_elf64
	$(BUILD)/test_boot_storage
	$(BUILD)/test_boot_config
	$(BUILD)/test_boot_health
	$(BUILD)/test_firmware_update
	$(PYTHON) tests/test_compaq610_preflight.py
	$(PYTHON) tests/test_compaq610_port_input.py
	$(PYTHON) tests/test_make_smoke_recipes.py

$(BUILD)/stage1.o: boot/stage1.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/stage1.bin: $(BUILD)/stage1.o
	$(LD) -m elf_i386 -Ttext 0x7C00 --oformat binary -e _start $< -o $@

$(BUILD)/stage2.o: boot/stage2.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/stage2_pm.o: boot/stage2_pm.c boot/elf64.h boot/protocol.h boot/core/block.h boot/core/partition.h boot/core/fat32.h boot/core/config.h boot/core/health.h | $(BUILD)
	$(CC) $(CFLAGS) -Iboot -c $< -o $@

$(BUILD)/elf64_pm.o: boot/elf64.c boot/elf64.h | $(BUILD)
	$(CC) $(CFLAGS) -Iboot -c $< -o $@

$(BUILD)/partition_pm.o: boot/core/partition.c boot/core/partition.h boot/core/block.h | $(BUILD)
	$(CC) $(CFLAGS) -Iboot/core -c $< -o $@

$(BUILD)/fat32_pm.o: boot/core/fat32.c boot/core/fat32.h boot/core/block.h boot/core/partition.h | $(BUILD)
	$(CC) $(CFLAGS) -Iboot/core -c $< -o $@

$(BUILD)/config_pm.o: boot/core/config.c boot/core/config.h | $(BUILD)
	$(CC) $(CFLAGS) -Iboot/core -c $< -o $@

$(BUILD)/health_pm.o: boot/core/health.c boot/core/health.h | $(BUILD)
	$(CC) $(CFLAGS) -Iboot/core -c $< -o $@

$(BUILD)/runtime32.o: boot/runtime32.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/bios_block.o: boot/bios_block.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/stage2.bin: $(BUILD)/stage2.o $(BUILD)/stage2_pm.o $(BUILD)/elf64_pm.o $(BUILD)/partition_pm.o $(BUILD)/fat32_pm.o $(BUILD)/config_pm.o $(BUILD)/health_pm.o $(BUILD)/runtime32.o $(BUILD)/bios_block.o
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
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/joshbios.img bs=512 seek=65 conv=notrunc status=none

check: image
	@test $$(wc -c < $(BUILD)/stage1.bin) -eq 512
	@sig=$$(od -An -tx1 -j510 -N2 $(BUILD)/stage1.bin | tr -d ' \n'); test "$$sig" = "55aa"
	@echo "JoshBIOS image OK: $(BUILD)/joshbios.img"

run: all
	qemu-system-i386 -drive format=raw,file=$(BUILD)/joshbios.img

smoke: all
	@rm -f $(BUILD)/boot.log; 	  status=0; 	  timeout 10s qemu-system-i386 	    -drive format=raw,file=$(BUILD)/joshbios.img 	    -display none -serial stdio -monitor none -no-reboot 	    > $(BUILD)/boot.log 2>&1 || status=$$?; 	  test $$status -eq 0 -o $$status -eq 124; 	  grep -q JOSHBIOS_BOOTINFO_OK $(BUILD)/boot.log; 	  grep -q JOSHBIOS_BOOT_OK $(BUILD)/boot.log; 	  echo "JoshBIOS QEMU boot smoke test passed."

bridge-image: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/josh-healthctl
	@test -f "$(ASHFALLEN_KERNEL)" || { echo "AshFallen kernel not found: $(ASHFALLEN_KERNEL)"; exit 1; }
	@kernel_size=$$(wc -c < "$(ASHFALLEN_KERNEL)"); max=$$(( $(KERNEL_SECTORS) * 512 )); 	  test $$kernel_size -le $$max || { echo "AshFallen kernel too large for loader buffer: $$kernel_size > $$max"; exit 1; }
	truncate -s 67108864 $(BRIDGE_IMAGE)
	dd if=$(BUILD)/stage1.bin of=$(BRIDGE_IMAGE) conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$(BRIDGE_IMAGE) bs=512 seek=1 conv=notrunc status=none
	printf '\200\000\000\000\014\000\000\000\000\010\000\000\000\370\001\000' | dd of=$(BRIDGE_IMAGE) bs=1 seek=446 conv=notrunc status=none
	mformat -i "$(BRIDGE_IMAGE)@@1048576" -F -v JOSHBOOT ::
	mmd -i "$(BRIDGE_IMAGE)@@1048576" ::/BOOT
	mmd -i "$(BRIDGE_IMAGE)@@1048576" ::/BOOT/JOSH
	printf '%s\n' \
		'version=1' \
		'default=josh' \
		'timeout=3' \
		'entry=josh' \
		'name=Josh OS' \
		'kernel=/boot/josh/kernel.elf' \
		'previous_kernel=/boot/josh/kernel-prev.elf' \
		'recovery_kernel=/boot/josh/recovery.elf' \
		'cmdline=josh.boot=normal' \
		'flags=development' > $(BUILD)/BOOT.CFG
	mcopy -i "$(BRIDGE_IMAGE)@@1048576" $(BUILD)/BOOT.CFG ::/BOOT/JOSH/BOOT.CFG
	mcopy -i "$(BRIDGE_IMAGE)@@1048576" "$(ASHFALLEN_KERNEL)" ::/BOOT/JOSH/KERNEL.ELF
	mcopy -i "$(BRIDGE_IMAGE)@@1048576" "$(ASHFALLEN_KERNEL)" ::/BOOT/JOSH/KERNEL-PREV.ELF
	mcopy -i "$(BRIDGE_IMAGE)@@1048576" "$(ASHFALLEN_KERNEL)" ::/BOOT/JOSH/RECOVERY.ELF
	$(BUILD)/josh-healthctl init $(BRIDGE_IMAGE) >/dev/null
	@echo "JoshBootloader FAT32 + AshFallen bridge image ready: $(BRIDGE_IMAGE)"

bridge-smoke: bridge-image
	@rm -f $(BUILD)/bridge.log; 	  status=0; 	  timeout 15s qemu-system-x86_64 	    -machine pc -m 256M 	    -drive format=raw,file=$(BRIDGE_IMAGE) 	    -display none -serial stdio -monitor none -no-reboot 	    > $(BUILD)/bridge.log 2>&1 || status=$$?; 	  test $$status -eq 0 -o $$status -eq 124; 	  grep -q JOSHBOOT_CPU_LONG_MODE_OK $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_PARTITION_OK $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_FAT32_OK $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_CONFIG_OK $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_KERNEL_PATH_OK $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_KERNEL_FILE_LOADED $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_ELF64_DETECTED $(BUILD)/bridge.log; 	  grep -q JOSHBOOT_HANDOFF_READY $(BUILD)/bridge.log; 	  grep -q JOSHOS_KERNEL_ENTERED $(BUILD)/bridge.log; 	  grep -q JOSHOS_BOOT_ADAPTER_OK $(BUILD)/bridge.log; 	  grep -q JOSHOS_RSDP_OK $(BUILD)/bridge.log; 	  grep -q JOSHOS_SMBIOS_OK $(BUILD)/bridge.log; 	  grep -q JOSHOS_CMDLINE_OK $(BUILD)/bridge.log; 	  grep -q JOSHOS_PAGING_OWNED_OK $(BUILD)/bridge.log; 	  grep -q JOSHOS_BOOT_OK $(BUILD)/bridge.log; 	  echo "JoshBootloader FAT32 -> AshFallen QEMU bridge smoke test passed."

bridge-config-fail-smoke: bridge-image
	cp $(BRIDGE_IMAGE) $(BAD_CONFIG_IMAGE)
	printf '%s\n' \
		'version=99' \
		'default=josh' \
		'timeout=3' \
		'entry=josh' \
		'name=Josh OS' \
		'kernel=/boot/josh/kernel.elf' > $(BUILD)/BAD.CFG
	mcopy -o -i "$(BAD_CONFIG_IMAGE)@@1048576" $(BUILD)/BAD.CFG ::/BOOT/JOSH/BOOT.CFG
	@rm -f $(BUILD)/bad-config.log; \
	  status=0; \
	  timeout 10s qemu-system-x86_64 \
	    -machine pc -m 256M \
	    -drive format=raw,file=$(BAD_CONFIG_IMAGE) \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/bad-config.log 2>&1 || status=$$?; \
	  test $$status -eq 0 -o $$status -eq 124; \
	  grep -q JOSHBOOT_ERROR_CONFIG_PARSE $(BUILD)/bad-config.log; \
	  grep -q JOSHBOOT_ERROR_FILESYSTEM_LOAD $(BUILD)/bad-config.log; \
	  ! grep -q JOSHOS_BOOT_OK $(BUILD)/bad-config.log; \
	  echo "JoshBootloader malformed-config QEMU smoke test passed."

bridge-rollback-smoke: bridge-image $(BUILD)/josh-healthctl
	cp $(BRIDGE_IMAGE) $(ROLLBACK_IMAGE)
	$(BUILD)/josh-healthctl init $(ROLLBACK_IMAGE) >/dev/null
	$(BUILD)/josh-healthctl good $(ROLLBACK_IMAGE) previous >/dev/null
	$(BUILD)/josh-healthctl pending $(ROLLBACK_IMAGE) current >/dev/null
	@set -e; \
	  for attempt in 1 2 3; do \
	    log="$(BUILD)/rollback-$$attempt.log"; \
	    status=0; \
	    timeout 15s qemu-system-x86_64 \
	      -machine pc -m 256M \
	      -drive format=raw,file=$(ROLLBACK_IMAGE) \
	      -display none -serial stdio -monitor none -no-reboot \
	      > "$$log" 2>&1 || status=$$?; \
	    test $$status -eq 0 -o $$status -eq 124; \
	    cat "$$log"; \
	    grep -q JOSHBOOT_SLOT_CURRENT "$$log"; \
	    grep -q JOSHOS_BOOT_OK "$$log"; \
	  done
	@status=0; \
	  timeout 15s qemu-system-x86_64 \
	    -machine pc -m 256M \
	    -drive format=raw,file=$(ROLLBACK_IMAGE) \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/rollback-4.log 2>&1 || status=$$?; \
	  test $$status -eq 0 -o $$status -eq 124; \
	  cat $(BUILD)/rollback-4.log; \
	  grep -q JOSHBOOT_HEALTH_ROLLBACK_PREVIOUS $(BUILD)/rollback-4.log; \
	  grep -q JOSHBOOT_SLOT_PREVIOUS $(BUILD)/rollback-4.log; \
	  grep -q JOSHOS_BOOT_OK $(BUILD)/rollback-4.log
	@$(BUILD)/josh-healthctl show $(ROLLBACK_IMAGE) > $(BUILD)/rollback-state.log
	@grep -q '^selected=previous$$' $(BUILD)/rollback-state.log
	@grep -q '^pending_good=0$$' $(BUILD)/rollback-state.log
	@echo "JoshBootloader persistent previous-good rollback smoke test passed."

bridge-recovery-smoke: bridge-image
	cp $(BRIDGE_IMAGE) $(RECOVERY_IMAGE)
	mdel -i "$(RECOVERY_IMAGE)@@1048576" ::/BOOT/JOSH/KERNEL.ELF
	mdel -i "$(RECOVERY_IMAGE)@@1048576" ::/BOOT/JOSH/KERNEL-PREV.ELF
	@rm -f $(BUILD)/recovery.log; \
	  status=0; \
	  timeout 15s qemu-system-x86_64 \
	    -machine pc -m 256M \
	    -drive format=raw,file=$(RECOVERY_IMAGE) \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/recovery.log 2>&1 || status=$$?; \
	  test $$status -eq 0 -o $$status -eq 124; \
	  cat $(BUILD)/recovery.log; \
	  grep -q JOSHBOOT_FALLBACK_PREVIOUS $(BUILD)/recovery.log; \
	  grep -q JOSHBOOT_FALLBACK_RECOVERY $(BUILD)/recovery.log; \
	  grep -q JOSHBOOT_SLOT_RECOVERY $(BUILD)/recovery.log; \
	  grep -q JOSHOS_BOOT_OK $(BUILD)/recovery.log; \
	  echo "JoshBootloader recovery-kernel fallback smoke test passed."

$(BUILD)/uefi_main.obj: boot/uefi/main.c boot/uefi/efi.h boot/elf64.h boot/protocol.h boot/core/config.h boot/core/health.h | $(BUILD)
	$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD)/uefi_handoff.obj: boot/uefi/handoff.S | $(BUILD)
	$(UEFI_CC) --target=x86_64-pc-win32-coff -ffreestanding -mno-red-zone -c $< -o $@

$(BUILD)/uefi_elf64.obj: boot/elf64.c boot/elf64.h | $(BUILD)
	$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD)/uefi_config.obj: boot/core/config.c boot/core/config.h | $(BUILD)
	$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD)/uefi_health.obj: boot/core/health.c boot/core/health.h | $(BUILD)
	$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(BUILD)/BOOTX64.EFI: $(BUILD)/uefi_main.obj $(BUILD)/uefi_handoff.obj $(BUILD)/uefi_elf64.obj $(BUILD)/uefi_config.obj $(BUILD)/uefi_health.obj
	$(UEFI_LD) /subsystem:efi_application /entry:efi_main /nodefaultlib /out:$@ $^

uefi: $(BUILD)/BOOTX64.EFI
	@file $(BUILD)/BOOTX64.EFI | grep -q 'for EFI (application)'
	@echo "JoshUEFI application OK: $(BUILD)/BOOTX64.EFI"

$(BUILD)/joshuefi.img: $(BUILD)/BOOTX64.EFI
	@test -f "$(ASHFALLEN_KERNEL)" || { echo "AshFallen kernel not found: $(ASHFALLEN_KERNEL)"; exit 1; }
	truncate -s 67108864 $@
	mformat -i $@ -F ::
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mmd -i $@ ::/EFI/JOSH
	mcopy -i $@ $(BUILD)/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	printf '%s\n' \
		'version=1' \
		'default=josh' \
		'timeout=3' \
		'entry=josh' \
		'name=Josh OS' \
		'kernel=/EFI/JOSH/KERNEL.ELF' \
		'previous_kernel=/EFI/JOSH/KERNEL-PREV.ELF' \
		'recovery_kernel=/EFI/JOSH/RECOVERY.ELF' \
		'cmdline=josh.boot=uefi' \
		'flags=development' > $(BUILD)/UEFI-BOOT.CFG
	mcopy -i $@ $(BUILD)/UEFI-BOOT.CFG ::/EFI/JOSH/BOOT.CFG
	mcopy -i $@ "$(ASHFALLEN_KERNEL)" ::/EFI/JOSH/KERNEL.ELF
	mcopy -i $@ "$(ASHFALLEN_KERNEL)" ::/EFI/JOSH/KERNEL-PREV.ELF
	mcopy -i $@ "$(ASHFALLEN_KERNEL)" ::/EFI/JOSH/RECOVERY.ELF

uefi-image: $(BUILD)/joshuefi.img
	@echo "JoshUEFI removable-media image OK: $(BUILD)/joshuefi.img"

uefi-smoke: uefi-image
	@test -n "$(OVMF_CODE)" || { echo "OVMF code image not found"; exit 1; }
	@test -n "$(OVMF_VARS)" || { echo "OVMF vars image not found"; exit 1; }
	@cp "$(OVMF_VARS)" $(BUILD)/OVMF_VARS.fd
	@rm -f $(BUILD)/uefi.log; \
	  status=0; \
	  timeout 20s qemu-system-x86_64 \
	    -machine q35 -m 256M \
	    -drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
	    -drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
	    -drive format=raw,file=$(BUILD)/joshuefi.img \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/uefi.log 2>&1 || status=$$?; \
	  test $$status -eq 0 -o $$status -eq 124; \
	  grep -q JOSHUEFI_ENTRY_OK $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_CONFIG_OK $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_SLOT_CURRENT $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_KERNEL_FILE_LOADED $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_ELF64_LOADED $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_GOP_OK $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_MEMORY_MAP_OK $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_EXIT_BOOT_SERVICES_OK $(BUILD)/uefi.log; \
	  grep -q JOSHUEFI_HANDOFF_READY $(BUILD)/uefi.log; \
	  grep -q JOSHOS_KERNEL_ENTERED $(BUILD)/uefi.log; \
	  grep -q JOSHOS_BOOT_ADAPTER_OK $(BUILD)/uefi.log; \
	  grep -q JOSHOS_BOOT_OK $(BUILD)/uefi.log; \
	  echo "JoshUEFI -> AshFallen OVMF boot smoke test passed."

uefi-recovery-smoke: uefi-image
	@test -n "$(OVMF_CODE)" || { echo "OVMF code image not found"; exit 1; }
	@test -n "$(OVMF_VARS)" || { echo "OVMF vars image not found"; exit 1; }
	cp $(BUILD)/joshuefi.img $(UEFI_RECOVERY_IMAGE)
	mdel -i $(UEFI_RECOVERY_IMAGE) ::/EFI/JOSH/KERNEL.ELF
	mdel -i $(UEFI_RECOVERY_IMAGE) ::/EFI/JOSH/KERNEL-PREV.ELF
	@cp "$(OVMF_VARS)" $(BUILD)/OVMF_RECOVERY_VARS.fd
	@rm -f $(BUILD)/uefi-recovery.log; \
	  status=0; \
	  timeout 20s qemu-system-x86_64 \
	    -machine q35 -m 256M \
	    -drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
	    -drive if=pflash,format=raw,file=$(BUILD)/OVMF_RECOVERY_VARS.fd \
	    -drive format=raw,file=$(UEFI_RECOVERY_IMAGE) \
	    -display none -serial stdio -monitor none -no-reboot \
	    > $(BUILD)/uefi-recovery.log 2>&1 || status=$$?; \
	  test $$status -eq 0 -o $$status -eq 124; \
	  grep -q JOSHUEFI_FALLBACK_PREVIOUS $(BUILD)/uefi-recovery.log; \
	  grep -q JOSHUEFI_FALLBACK_RECOVERY $(BUILD)/uefi-recovery.log; \
	  grep -q JOSHUEFI_SLOT_RECOVERY $(BUILD)/uefi-recovery.log; \
	  grep -q JOSHOS_BOOT_ADAPTER_OK $(BUILD)/uefi-recovery.log; \
	  grep -q JOSHOS_BOOT_OK $(BUILD)/uefi-recovery.log; \
	  echo "JoshUEFI recovery-kernel fallback smoke test passed."

clean:
	rm -rf $(BUILD)
