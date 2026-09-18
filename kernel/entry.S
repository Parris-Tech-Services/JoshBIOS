.code32
.section .text.entry
.global _start
.extern kmain
_start:
    cli
    movl $0x90000, %esp

    # Stage 2 passes a JoshBootInfo pointer in EAX.
    pushl %eax
    call kmain
    addl $4, %esp
1:
    hlt
    jmp 1b
