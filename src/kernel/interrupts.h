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

typedef enum QWERTY_ES_ASCII_Table_Index
{
    Key_One = 2,
    Key_Two = 3,
    Key_Three = 4,
    Key_Four = 5,
    Key_Five = 6,
    Key_Six = 7,
    Key_Seven = 8,
    Key_Eight = 9,
    Key_Nine = 10,
    Key_Zero = 11,
    Key_SingleQuote   = 12,
    Key_Equal = 13, //@tricky
    Key_Backspace = 14,
    Key_Q    = 16,
    Key_W    = 17,
    Key_E    = 18,
    Key_R    = 19,
    Key_T    = 20,
    Key_Y    = 21,
    Key_U    = 22,
    Key_I    = 23,
    Key_O    = 24,
    Key_P    = 25,
    Key_LeftBracket = 26, // @tricky
    Key_Plus = 27,
    Key_Enter = 28,
    Key_A    = 30,
    Key_S    = 31,
    Key_D    = 32,
    Key_F    = 33,
    Key_G    = 34,
    Key_H    = 35,
    Key_J    = 36,
    Key_K    = 37,
    Key_L    = 38,
    Key_N    = 39, // @tricky
    KEY_CEDILLA = 41,
    Key_LeftShift = 42,
    Key_LessThan = 43,
    Key_Z    = 44,
    Key_X    = 45,
    Key_C    = 46,
    Key_V    = 47,
    Key_B    = 48,
    Key_N_SPA= 49,
    Key_M    = 50,
    Key_Comma = 51,
    Key_Dot   = 52,
    Key_Dash  = 53,
    Key_RightShift = 54,
    Key_Space = 57,
} QWERTY_ES_ASCII_Table_Index;

typedef struct KeyboardBuffer
{
    u8 messages[256];
} KeyboardBuffer;

struct InterruptFrame;
extern u8 mouse_cycle;
extern u8 mouse_packet[4];
extern bool mouse_packet_ready;

void PIC_remap(void);
void PS2_mouse_init(void);
bool kb_get_buffer(KeyboardBuffer* out_kb_buffer, u16* out_kb_event_count);
char translate_scancode(u8 scancode, bool uppercase);

INTERRUPT_HANDLER void page_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void double_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void general_protection_fault_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void keyboard_handler(struct InterruptFrame* frame);
INTERRUPT_HANDLER void mouse_handler(struct InterruptFrame* frame);

void interrupts_setup(void);
void PS2_mouse_init(void);