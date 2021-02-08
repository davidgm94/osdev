#pragma once

#include "types.h"

enum 
{
    PIC1_COMMAND = 0x20,
    PIC1_DATA = 0x21,
    PIC2_COMMAND = 0xA0,
    PIC2_DATA = 0xA1,
    PIC_EOI = 0x20,

    ICW1_INIT = 0x10,
    ICW1_ICW4 = 0x01,
    ICW4_8086 = 0x01,
};

struct InterruptFrame;

void PIC_remap(void);
INTERRUPT_HANDLER void page_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void double_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void general_protection_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void keyboard_handler(struct InterruptFrame* frame);
