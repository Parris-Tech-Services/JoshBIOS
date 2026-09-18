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

    # Load stage 2: 16 sectors beginning at LBA/CHS sector 2.
    movb $0x02, %ah
    movb $16, %al
    movb $0x00, %ch
    movb $0x02, %cl
    movb $0x00, %dh
    movb boot_drive, %dl
    movw $0x8000, %bx
    int $0x13
    jc disk_error

    ljmp $0x0000, $0x8000

disk_error:
    movw $disk_msg, %si
    call print_string
1:
    cli
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

banner:
    .asciz "JoshBIOS stage 1\r\n"
disk_msg:
    .asciz "Disk read failed\r\n"
boot_drive:
    .byte 0

.org 510
.word 0xaa55
