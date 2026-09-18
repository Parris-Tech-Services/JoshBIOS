.code16
.section .text
.global _start
.global enter_long_mode
.global e820_count
.global boot_drive
.extern stage2_pm_main
.extern stage2_kernel_entry

.equ BOOT_TIMEOUT_TICKS, 55
.equ KERNEL_LBA, 33
.equ KERNEL_CHUNKS, 8
.equ KERNEL_SEGMENT_START, 0x1000
.equ E820_SEGMENT, 0x5900
.equ VBE_SEGMENT, 0x5a00
.equ PML4_ADDR, 0x50000
.equ BOOTINFO_ADDR, 0x58000

_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7a00, %sp
    sti

    movb %dl, boot_drive
    call show_boot_menu

boot_kernel:
    call clear_screen
    movw $booting_msg, %si
    call print_string

    call check_edd
    jc edd_error
    call load_kernel_extent
    jc disk_error

    # The existing 32-bit test kernel remains a fallback. Only the ELF64 path
    # needs platform discovery and the long-mode hand-off.
    movw $0x1000, %ax
    movw %ax, %es
    cmpl $0x464c457f, %es:0
    jne enter_protected_mode

    xorw %ax, %ax
    movw %ax, %es
    call collect_e820
    jc e820_error
    call setup_vbe
    jc vbe_error

enter_protected_mode:
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

load_kernel_extent:
    movw $KERNEL_SEGMENT_START, kernel_segment
    movl $KERNEL_LBA, kernel_lba
    movw $KERNEL_CHUNKS, %cx
1:
    pushw %cx
    movw $kernel_packet, %si
    movb boot_drive, %dl
    movb $0x42, %ah
    int $0x13
    jc 2f
    addw $0x0800, kernel_segment
    addl $64, kernel_lba
    popw %cx
    loop 1b
    clc
    ret
2:
    popw %cx
    stc
    ret

collect_e820:
    pushw %es
    pushw %di
    movw $E820_SEGMENT, %ax
    movw %ax, %es
    xorw %di, %di
    movw $0, e820_count
    xorl %ebx, %ebx
1:
    movl $0, %es:20(%di)
    movl $0xe820, %eax
    movl $0x534d4150, %edx
    movl $24, %ecx
    int $0x15
    jc 3f
    cmpl $0x534d4150, %eax
    jne 4f
    cmpw $20, %cx
    jb 4f
    incw e820_count
    cmpw $128, e820_count
    jae 2f
    testl %ebx, %ebx
    jz 2f
    addw $24, %di
    jmp 1b
2:
    popw %di
    popw %es
    clc
    ret
3:
    cmpw $0, e820_count
    jne 2b
4:
    popw %di
    popw %es
    stc
    ret

try_vbe_mode:
    pushw %es
    pushw %di
    movw $VBE_SEGMENT, %ax
    movw %ax, %es
    xorw %di, %di
    movw $0x4f01, %ax
    int $0x10
    cmpw $0x004f, %ax
    jne 1f
    testw $0x0080, %es:0(%di)
    jz 1f
    cmpb $32, %es:0x19(%di)
    jne 1f
    cmpb $6, %es:0x1b(%di)
    jne 1f
    cmpl $0, %es:0x28(%di)
    je 1f
    popw %di
    popw %es
    clc
    ret
1:
    popw %di
    popw %es
    stc
    ret

setup_vbe:
    movw $0x0144, %cx
    movw %cx, vbe_selected_mode
    call try_vbe_mode
    jnc 1f
    movw $0x0143, %cx
    movw %cx, vbe_selected_mode
    call try_vbe_mode
    jnc 1f
    movw $0x0142, %cx
    movw %cx, vbe_selected_mode
    call try_vbe_mode
    jc 2f
1:
    movw vbe_selected_mode, %bx
    orw $0x4000, %bx
    movw $0x4f02, %ax
    int $0x10
    cmpw $0x004f, %ax
    jne 2f
    clc
    ret
2:
    stc
    ret

show_boot_menu:
    call clear_screen
    movw $banner, %si
    call print_string
    movw $menu, %si
    call print_string
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
    jmp halt
e820_error:
    movw $e820_msg, %si
    call print_string
    jmp halt
vbe_error:
    movw $vbe_msg, %si
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
kernel_packet:
    .byte 0x10
    .byte 0x00
    .word 64
    .word 0x0000
kernel_segment:
    .word KERNEL_SEGMENT_START
kernel_lba:
    .quad KERNEL_LBA

.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00af9a000000ffff
gdt_end:
gdt_descriptor:
    .word gdt_end - gdt - 1
    .long gdt

banner:
    .asciz "JoshBIOS Boot Manager 0.3\r\n=========================\r\n\r\n"
menu:
    .asciz "[Enter/B] Boot Josh kernel\r\n[D] Diagnostics\r\n[R] Reboot\r\n\r\nAuto-booting in about 3 seconds...\r\n"
booting_msg: .asciz "Starting Josh kernel...\r\n"
diag_title: .asciz "JoshBIOS Diagnostics\r\n====================\r\n"
diag_drive: .asciz "Boot drive: 0x"
diag_edd: .asciz "BIOS EDD extensions: "
diag_yes: .asciz "available\r\n"
diag_no: .asciz "not available\r\n"
diag_abi: .asciz "Josh Boot Protocol: v0.1 experimental\r\n"
diag_continue: .asciz "\r\nPress any key to return to the boot menu.\r\n"
edd_msg: .asciz "BIOS EDD extensions unavailable.\r\n"
disk_msg: .asciz "Kernel extent read failed.\r\n"
e820_msg: .asciz "E820 memory map unavailable.\r\n"
vbe_msg: .asciz "32-bpp VBE framebuffer unavailable.\r\n"
newline: .asciz "\r\n"
boot_drive: .byte 0
start_ticks: .word 0
e820_count: .word 0
vbe_selected_mode: .word 0

.code32
protected_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    movl $0x90000, %esp
    call stage2_pm_main
    movl $0x10000, %edx
    jmp *%edx

enter_long_mode:
    cli
    movl $PML4_ADDR, %eax
    movl %eax, %cr3
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4
    movl $0xc0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0
    ljmpl $0x18, $long_mode_entry

.code64
long_mode_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movabs $0x90000, %rsp
    xorq %rbp, %rbp
    movabs $0x544f4f4248534f4a, %rdi
    movabs $0x0030564f544f5250, %rsi
    movabs $BOOTINFO_ADDR, %rdx
    movl $stage2_kernel_entry, %eax
    movq (%rax), %rax
    jmp *%rax
