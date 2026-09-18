CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
BUILD := build
STAGE2_SECTORS := 16
KERNEL_SECTORS := 64

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -Wall -Wextra -Werror -O2
ASFLAGS := -m32 -ffreestanding -fno-pie

.PHONY: all clean image check run smoke
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
	@stage2_size=$$(wc -c < $(BUILD)/stage2.bin); max=$$(( $(STAGE2_SECTORS) * 512 )); 	  test $$stage2_size -le $$max || { echo "stage2 too large: $$stage2_size > $$max"; exit 1; }
	@kernel_size=$$(wc -c < $(BUILD)/kernel.bin); max=$$(( $(KERNEL_SECTORS) * 512 )); 	  test $$kernel_size -le $$max || { echo "kernel too large: $$kernel_size > $$max"; exit 1; }
	truncate -s 1048576 $(BUILD)/joshbios.img
	dd if=$(BUILD)/stage1.bin of=$(BUILD)/joshbios.img conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$(BUILD)/joshbios.img bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/joshbios.img bs=512 seek=17 conv=notrunc status=none

check: image
	@test $$(wc -c < $(BUILD)/stage1.bin) -eq 512
	@sig=$$(od -An -tx1 -j510 -N2 $(BUILD)/stage1.bin | tr -d ' \n'); test "$$sig" = "55aa"
	@echo "JoshBIOS image OK: $(BUILD)/joshbios.img"

run: all
	qemu-system-i386 -drive format=raw,file=$(BUILD)/joshbios.img

smoke: all
	@rm -f $(BUILD)/boot.log; 	  status=0; 	  timeout 10s qemu-system-i386 	    -drive format=raw,file=$(BUILD)/joshbios.img 	    -display none -serial stdio -monitor none -no-reboot 	    > $(BUILD)/boot.log 2>&1 || status=$$?; 	  test $$status -eq 0 -o $$status -eq 124; 	  grep -q JOSHBIOS_BOOTINFO_OK $(BUILD)/boot.log; 	  grep -q JOSHBIOS_BOOT_OK $(BUILD)/boot.log; 	  echo "JoshBIOS QEMU boot smoke test passed."

clean:
	rm -rf $(BUILD)
