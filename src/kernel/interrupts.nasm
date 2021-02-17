[bits 64]
[extern ISR]

%macro pushaq 0
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popaq 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

%macro ISR 2
    GLOBAL isr%1
    isr%1:
        pushaq
        xor ax, ax
        mov es, ax
        mov ds, ax
        cld
        mov rdi, rsp
        mov rax, [ISR + 8 * %1]
        call rax
        popaq
        add rsp, 16
        iretq
%endmacro

section .text
align 16

ISR 0, 0
ISR 1, 0
ISR 2, 0
ISR 3, 0
ISR 4, 0
ISR 5, 0
ISR 6, 0
ISR 7, 0
ISR 8, 1
ISR 9, 0
ISR 10, 1
ISR 11, 1
ISR 12, 1
ISR 13, 1
ISR 14, 1
ISR 16, 0
ISR 17, 1
ISR 18, 0
ISR 19, 0
ISR 20, 0
ISR 30, 1
ISR 33, 0