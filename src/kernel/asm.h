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
