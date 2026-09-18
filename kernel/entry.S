.code32
.section .text.entry
.global _start
.extern kmain
_start:
    cli
    movl $0x90000, %esp
    call kmain
1:
    hlt
    jmp 1b
