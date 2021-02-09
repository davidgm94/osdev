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

enum MouseData
{
    PS2XSign        =   0b00010000,
    PS2YSign        =   0b00100000,
    PS2XOverflow    =   0b01000000,
    PS2YOverflow    =   0b10000000,

    PS2LeftButton   =   0b00000001,
    PS2RightButton  =   0b00000010,
    PS2MiddleButton =   0b00000100,
};

struct InterruptFrame;
extern u8 mouse_cycle;
extern u8 mouse_packet[4];
extern bool mouse_packet_ready;

void PIC_remap(void);
void PS2_mouse_init(void);
INTERRUPT_HANDLER void page_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void double_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void general_protection_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void keyboard_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void mouse_handler(struct InterruptFrame* frame);
