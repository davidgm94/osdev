section .text
align 16

extern KernelMain

global _start
_start:
    cli
    xor rbp, rbp ; set rbp to NULL just to properly trace the stack
    call KernelMain

.hang:
    hlt
    jmp .hang
.end:
