.code16
.section .text
.global _start

.equ BOOTINFO_ADDR,  0x7000
.equ BOOTINFO_MAGIC, 0x48534f4a
.equ BOOTINFO_SIZE,  16
.equ BOOT_TIMEOUT_TICKS, 55

_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7a00, %sp
    sti

    movb %dl, boot_drive
    call build_boot_info
    call show_boot_menu

boot_kernel:
    call clear_screen
    movw $booting_msg, %si
    call print_string

    call check_edd
    jc edd_error

    # Load the kernel from LBA 17 into physical address 0x10000.
    movw $disk_packet, %si
    movb boot_drive, %dl
    movb $0x42, %ah
    int $0x13
    jc disk_error

    # Fast A20 gate.
    inb $0x92, %al
    orb $0x02, %al
    andb $0xfe, %al
    outb %al, $0x92

    cli
    lgdt gdt_descriptor
    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $protected_mode

show_boot_menu:
    call clear_screen
    movw $banner, %si
    call print_string
    movw $menu, %si
    call print_string

    # BIOS timer ticks at about 18.2 Hz. 55 ticks is roughly three seconds.
    xorw %ax, %ax
    int $0x1a
    movw %dx, start_ticks

menu_wait:
    movb $0x01, %ah
    int $0x16
    jnz menu_key

    xorw %ax, %ax
    int $0x1a
    movw %dx, %ax
    subw start_ticks, %ax
    cmpw $BOOT_TIMEOUT_TICKS, %ax
    jb menu_wait
    ret

menu_key:
    xorw %ax, %ax
    int $0x16
    cmpb $0x0d, %al
    je menu_boot
    cmpb $'b', %al
    je menu_boot
    cmpb $'B', %al
    je menu_boot
    cmpb $'d', %al
    je menu_diagnostics
    cmpb $'D', %al
    je menu_diagnostics
    cmpb $'r', %al
    je menu_reboot
    cmpb $'R', %al
    je menu_reboot
    jmp menu_wait

menu_boot:
    ret

menu_diagnostics:
    call diagnostics
    jmp show_boot_menu

menu_reboot:
    int $0x19
    jmp menu_reboot

diagnostics:
    call clear_screen
    movw $diag_title, %si
    call print_string
    movw $diag_drive, %si
    call print_string
    movb boot_drive, %al
    call print_hex8
    movw $newline, %si
    call print_string

    movw $diag_edd, %si
    call print_string
    call check_edd
    jc diag_no_edd
    movw $diag_yes, %si
    call print_string
    jmp diag_done_edd

diag_no_edd:
    movw $diag_no, %si
    call print_string

diag_done_edd:
    movw $diag_abi, %si
    call print_string
    movw $diag_continue, %si
    call print_string
    xorw %ax, %ax
    int $0x16
    ret

build_boot_info:
    movw $BOOTINFO_ADDR, %di
    movl $BOOTINFO_MAGIC, 0(%di)
    movw $1, 4(%di)
    movw $BOOTINFO_SIZE, 6(%di)
    movb boot_drive, %al
    movb %al, 8(%di)
    movb $1, 9(%di)       # firmware type 1 = legacy BIOS
    movw $0, 10(%di)      # flags
    movl $0, 12(%di)      # reserved
    ret

check_edd:
    movb boot_drive, %dl
    movw $0x55aa, %bx
    movb $0x41, %ah
    int $0x13
    jc 1f
    cmpw $0xaa55, %bx
    jne 1f
    testw $0x0001, %cx
    jz 1f
    clc
    ret
1:
    stc
    ret

clear_screen:
    movw $0x0003, %ax
    int $0x10
    ret

print_hex8:
    pushw %ax
    movb %al, %ah
    shrb $4, %al
    call print_nibble
    movb %ah, %al
    andb $0x0f, %al
    call print_nibble
    popw %ax
    ret

print_nibble:
    andb $0x0f, %al
    cmpb $10, %al
    jb 1f
    addb $('A' - 10), %al
    jmp 2f
1:
    addb $'0', %al
2:
    movb $0x0e, %ah
    movb $0x00, %bh
    movb $0x07, %bl
    int $0x10
    ret

edd_error:
    movw $edd_msg, %si
    call print_string
    jmp halt

disk_error:
    movw $disk_msg, %si
    call print_string

halt:
    cli
1:
    hlt
    jmp 1b

print_string:
    lodsb
    testb %al, %al
    jz 2f
    movb $0x0e, %ah
    movb $0x00, %bh
    movb $0x07, %bl
    int $0x10
    jmp print_string
2:
    ret

.align 4
disk_packet:
    .byte 0x10
    .byte 0x00
    .word 64
    .word 0x0000
    .word 0x1000
    .quad 17

.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt - 1
    .long gdt

banner:
    .asciz "JoshBIOS Boot Manager 0.2\r\n=========================\r\n\r\n"
menu:
    .asciz "[Enter/B] Boot Josh kernel\r\n[D] Diagnostics\r\n[R] Reboot\r\n\r\nAuto-booting in about 3 seconds...\r\n"
booting_msg:
    .asciz "Starting Josh kernel...\r\n"
diag_title:
    .asciz "JoshBIOS Diagnostics\r\n====================\r\n"
diag_drive:
    .asciz "Boot drive: 0x"
diag_edd:
    .asciz "BIOS EDD extensions: "
diag_yes:
    .asciz "available\r\n"
diag_no:
    .asciz "not available\r\n"
diag_abi:
    .asciz "JoshBootInfo ABI: v1 @ 0x7000\r\n"
diag_continue:
    .asciz "\r\nPress any key to return to the boot menu.\r\n"
edd_msg:
    .asciz "BIOS EDD extensions unavailable.\r\n"
disk_msg:
    .asciz "Kernel load failed.\r\n"
newline:
    .asciz "\r\n"
boot_drive:
    .byte 0
start_ticks:
    .word 0

.code32
protected_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    movl $0x90000, %esp
    movl $BOOTINFO_ADDR, %eax
    movl $0x10000, %edx
    jmp *%edx
