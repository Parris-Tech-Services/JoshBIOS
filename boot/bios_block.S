.section .text
.global bios_block_read
.extern boot_drive
.extern gdt_descriptor

.code32
bios_block_read:
    pushl %ebp
    movl %esp, %ebp
    pushal

    # cdecl: context, uint64_t lba, uint32_t count, void *buffer
    movl 20(%ebp), %ecx
    testl %ecx, %ecx
    jz bios_argument_error
    cmpl $127, %ecx
    ja bios_argument_error

    movl 24(%ebp), %eax
    cmpl $0x00100000, %eax
    jae bios_argument_error

    movl %ecx, %edx
    shll $9, %edx
    addl %eax, %edx
    jc bios_argument_error
    cmpl $0x00100000, %edx
    ja bios_argument_error

    movl 12(%ebp), %edx
    movl %edx, bios_dap_lba
    movl 16(%ebp), %edx
    movl %edx, bios_dap_lba + 4

    movw %cx, bios_dap_count

    movl %eax, %edx
    andl $0x0f, %eax
    shrl $4, %edx
    movw %ax, bios_dap_offset
    movw %dx, bios_dap_segment

    movl %esp, bios_saved_esp
    ljmp $0x20, $bios_pm16_entry

bios_pm32_resume:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    movl bios_saved_esp, %esp
    popal

    movzbl bios_status, %eax
    testl %eax, %eax
    jz 1f
    movl $-1, %eax
    jmp 2f
1:
    xorl %eax, %eax
2:
    leave
    ret

bios_argument_error:
    popal
    movl $-1, %eax
    leave
    ret

.code16
bios_pm16_entry:
    movw $0x28, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7000, %sp

    movl %cr0, %eax
    andl $0xfffffffe, %eax
    movl %eax, %cr0
    ljmp $0x0000, $bios_real_mode

bios_real_mode:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7000, %sp
    sti

    movw $bios_dap, %si
    movb boot_drive, %dl
    movb $0x42, %ah
    int $0x13
    jc 1f
    movb $0, bios_status
    jmp 2f
1:
    movb $1, bios_status
2:
    cli
    lgdt gdt_descriptor
    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $bios_pm32_resume

.section .data
.align 4
bios_saved_esp:
    .long 0
bios_status:
    .byte 1
.align 4
bios_dap:
    .byte 0x10
    .byte 0x00
bios_dap_count:
    .word 1
bios_dap_offset:
    .word 0
bios_dap_segment:
    .word 0
bios_dap_lba:
    .quad 0
