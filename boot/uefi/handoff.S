.text
.code64
.global josh_uefi_handoff
josh_uefi_handoff:
    # Microsoft x64 ABI: rcx=root, rdx=stack, r8=entry, r9=boot info.
    cli
    movq %rcx, %cr3
    movq %rdx, %rsp
    xorq %rbp, %rbp
    movq %r8, %rax
    movabsq $0x544f4f4248534f4a, %rdi
    movabsq $0x0030564f544f5250, %rsi
    movq %r9, %rdx
    jmp *%rax
