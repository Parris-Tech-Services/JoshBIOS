.code16
.section .text
.global _start
_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7a00, %sp
    sti

    movb %dl, boot_drive
    movw $banner, %si
    call print_string

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
    .asciz "JoshBIOS stage 2\r\n"
disk_msg:
    .asciz "Kernel load failed\r\n"
boot_drive:
    .byte 0

.code32
protected_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    movl $0x90000, %esp
    movl $0x10000, %eax
    jmp *%eax
