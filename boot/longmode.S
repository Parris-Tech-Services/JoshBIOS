/*
 * longmode.S - 32-bit protected mode -> 64-bit long mode transition.
 */
    .code32
    .section .text
    .global josh_enter_long_mode
    .type josh_enter_long_mode, @function
josh_enter_long_mode:
    cli
    movl    4(%esp),  %eax
    movl    8(%esp),  %ebx
    movl    12(%esp), %ecx
    movl    16(%esp), %edx
    movl    %ebx, entry_lo
    movl    %ecx, entry_hi
    movl    %edx, boot_info_ptr
    movl    %cr4, %ecx
    orl     $((1 << 5) | (1 << 7)), %ecx
    movl    %ecx, %cr4
    movl    %eax, %cr3
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $((1 << 8) | (1 << 11)), %eax
    wrmsr
    movl    %cr0, %eax
    orl     $((1 << 31) | (1 << 16) | 1), %eax
    movl    %eax, %cr0
    lgdt    gdt64_descriptor
    ljmp    $0x08, $long_mode_entry
    .code64
long_mode_entry:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    movq    $0x00090000, %rsp
    xorq    %rbp, %rbp
    movl    boot_info_ptr(%rip), %edi
    movl    entry_lo(%rip), %eax
    movl    entry_hi(%rip), %edx
    shlq    $32, %rdx
    orq     %rdx, %rax
    pushq   $0
    jmpq    *%rax
.hang64:
    cli
    hlt
    jmp     .hang64
    .section .data
    .align 16
entry_lo:       .long 0
entry_hi:       .long 0
boot_info_ptr:  .long 0
    .align 16
gdt64:
    .quad 0
    .word 0, 0
    .byte 0
    .byte 0x9A
    .byte 0x20
    .byte 0
    .word 0, 0
    .byte 0
    .byte 0x92
    .byte 0x00
    .byte 0
gdt64_end:
    .align 16
gdt64_descriptor:
    .word gdt64_end - gdt64 - 1
    .long gdt64
