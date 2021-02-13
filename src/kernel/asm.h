#pragma once
#include "types.h"

static inline void outb(u16 port, u8 value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port)
{
    u8 result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void io_wait(void)
{
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

static inline void hlt(void)
{
    asm volatile("hlt");
}

static inline void loop_forever(void)
{
    for(;;)
    {
        hlt();
    }
}

static inline u64 rdmsr(u64 msr)
{
    u64 rax, rdx;

    asm volatile("rdmsr" : "=a"(rax), "=d"(rdx) : "c"(msr));

    return (rdx << 32) | rax;
}

static inline void wrmsr(u64 msr, u64 data)
{
    u64 rax = data & 0xFFFFFFFF;
    u64 rdx = data >> 32;
    asm volatile("wrmsr" :: "a"(rax), "d"(rdx), "c"(msr));
}

static inline void interrupts_enable(void)
{
    asm("sti");
}
static inline void interrupts_disable(void)
{
    asm("cli");
}
