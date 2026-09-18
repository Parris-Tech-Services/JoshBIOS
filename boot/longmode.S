/*
 * longmode.S - 32-bit protected mode -> 64-bit long mode transition.
 *
 * Called from stage3 as:
 *   josh_enter_long_mode(pml4_phys, entry_point, boot_info_phys)
 *
 * This function does not return. On entry to the kernel:
 *   RDI = physical address of the JoshBootInfo structure (SysV arg 0)
 *   RSP = a small bootstrap stack; the kernel is expected to switch
 *   CS  = 0x08 (64-bit code), DS/ES/FS/GS/SS = 0x10
 *   Interrupts disabled, paging on with the first 4 GiB identity mapped
 *
 * Ordering matters and is unforgiving: PAE must be set before CR0.PG,
 * EFER.LME must be set before CR0.PG, and CR3 must be valid before
 * CR0.PG. Get any of them wrong and the CPU triple-faults with no
 * diagnostic whatsoever.
 */
    .code32
    .section .text
    .global josh_enter_long_mode
    .type josh_enter_long_mode, @function

/* void josh_enter_long_mode(uint32_t pml4, uint64_t entry, uint32_t info) */
josh_enter_long_mode:
    cli

    movl    4(%esp),  %eax          /* pml4 physical address            */
    movl    8(%esp),  %ebx          /* entry point, low 32 bits         */
    movl    12(%esp), %ecx          /* entry point, high 32 bits        */
    movl    16(%esp), %edx          /* boot info physical address       */

    movl    %ebx, entry_lo
    movl    %ecx, entry_hi
    movl    %edx, boot_info_ptr

    /* CR4.PAE (bit 5). Long mode requires PAE paging; there is no
     * non-PAE long mode. Also set PGE (bit 7) so global pages work. */
    movl    %cr4, %ecx
    orl     $((1 << 5) | (1 << 7)), %ecx
    movl    %ecx, %cr4

    /* CR3 = PML4 physical address. */
    movl    %eax, %cr3

    /* EFER.LME (bit 8) via MSR 0xC0000080. Also set NXE (bit 11) so the
     * kernel may use the no-execute bit without a further MSR write. */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $((1 << 8) | (1 << 11)), %eax
    wrmsr

    /* CR0.PG (bit 31) | CR0.PE (bit 0) | CR0.WP (bit 16).
     * WP makes the kernel's later read-only mappings actually apply to
     * ring 0, which is nearly always what is wanted. */
    movl    %cr0, %eax
    orl     $((1 << 31) | (1 << 16) | 1), %eax
    movl    %eax, %cr0

    /* We are now in IA-32e compatibility mode. The far jump into a
     * 64-bit code segment is what actually enters 64-bit mode. */
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

    /* Bootstrap stack. The kernel should move to its own immediately;
     * this only needs to survive the first few instructions. */
    movq    $0x00090000, %rsp
    xorq    %rbp, %rbp

    /* SysV AMD64: first integer argument in RDI. */
    movl    boot_info_ptr(%rip), %edi

    movl    entry_lo(%rip), %eax
    movl    entry_hi(%rip), %edx
    shlq    $32, %rdx
    orq     %rdx, %rax

    /* Push a fake return address of 0 so a kernel that mistakenly
     * returns faults immediately and visibly rather than wandering. */
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
    .quad 0                          /* null descriptor                 */
    /* 0x08: 64-bit code. L=1, D=0, present, ring 0, executable.        */
    .word 0, 0
    .byte 0
    .byte 0x9A
    .byte 0x20                       /* L bit set, D clear              */
    .byte 0
    /* 0x10: data. In long mode base/limit are ignored, but the segment
     * must still be present and writable.                              */
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
