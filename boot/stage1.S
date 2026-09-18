.code16
.section .text
.global _start
_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    sti

    movb %dl, boot_drive
    movw $banner, %si
    call print_string

    call check_edd
    jc edd_error

    movw $stage2_packet, %si
    movb boot_drive, %dl
    movb $0x42, %ah
    int $0x13
    jc disk_error

    movb boot_drive, %dl
    ljmp $0x0000, $0x8000

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
stage2_packet:
    .byte 0x10
    .byte 0x00
    .word 32
    .word 0x8000
    .word 0x0000
    .quad 1

banner:
    .asciz "JoshBIOS stage 1\r\n"
edd_msg:
    .asciz "BIOS EDD unavailable\r\n"
disk_msg:
    .asciz "Stage 2 read failed\r\n"
boot_drive:
    .byte 0

.org 446
.space 64
.word 0xaa55
