#include "interrupts.h"
#include "asm.h"

extern void panic(const char*);
extern void println(const char*);
extern void print(const char*);
extern void putc(char);
extern void clear_char(void);
extern const char* unsigned_to_string(u64);
extern const char* hex_to_string(u64);
extern void println_hex(u64);
extern void print_hex(u64);

void handle_keyboard(u8 scancode);

typedef struct StackFrame
{
    struct StackFrame* rbp;
    u64 rip;
} StackFrame;

void stacktrace(u32 max_frame_count)
{
    u32 frame = 0;
    println("Stacktrace:");

    void* frame_addresses[] = 
    {
        __builtin_frame_address(0),
        __builtin_frame_address(1),
        __builtin_frame_address(2),
        __builtin_frame_address(3),
        __builtin_frame_address(4),
    };

    for (u32 i = 0; i < array_length(frame_addresses); i++)
    {
        print(unsigned_to_string(i));
        print(": ");
        println_hex((u64)frame_addresses[i]);
    }
}

void stacktrace_asm(u32 max_frame_count)
{
    u64* rbp;
    asm("movq %%rbp, %0;" : "=r"(rbp) ::);
    println("Stacktrace");
    println("==========");
    u32 i = 0;
    while (rbp)
    {
        u64 rip = *(rbp + 1);
        u64 next_rbp = *(rbp + 0);
        print("Call ");
        print(unsigned_to_string(i++));
        print(": ");
        println_hex(rip);
        rbp = (u64*)next_rbp;
    }
}

enum
{
    PS2_KEYBOARD_PORT = 0x60,
};

void PIC_end_master(void);
void PIC_end_slave(void);
void PIC_remap(void);

typedef struct KeyboardBitmap
{
    u64 values;
} KeyboardBitmap;

static KeyboardBitmap keymap = {0};

static inline bool KeyboardBitmap_get_value(KeyboardBitmap keymap, u64 index)
{
    if (index < 64)
    {
        return (keymap.values & index) >> index;
    }

    print("Wrong value for key: ");
    println(unsigned_to_string(index));
    loop_forever();
    return false;
}

static inline void KeyboardBitmap_set_value(KeyboardBitmap* keymap, u64 index, bool enabled)
{
    if (index < 64)
    {
        u64 bit_mask = (u64)1 << index;
        keymap->values &= ~bit_mask;
        keymap->values |= bit_mask * enabled;
    }

    /*print("Wrong value for key: ");*/
    /*println(unsigned_to_string(index));*/
    /*loop_forever();*/
}

INTERRUPT_HANDLER void page_fault_handler(struct InterruptFrame* frame)
{
    panic("Page fault detected");
    for(;;);
}

INTERRUPT_HANDLER void double_fault_handler(struct InterruptFrame* frame)
{
    panic("Double fault detected");
    for(;;);
}

INTERRUPT_HANDLER void general_protection_fault_handler(struct InterruptFrame* frame)
{
    panic("General protection fault detected");
    stacktrace_asm(10);
    for(;;);
}

static void key_release(u8 scancode);
static void key_press(u8 scancode);


KeyboardBuffer kb_buffer;
u16 kb_event_count;
bool overflow = false;

bool kb_get_buffer(KeyboardBuffer* out_kb_buffer, u16* out_kb_event_count)
{
    *out_kb_buffer = kb_buffer;
    *out_kb_event_count = kb_event_count;
    kb_buffer = (const KeyboardBuffer){0};
    kb_event_count = 0;
    return overflow;
}

INTERRUPT_HANDLER void keyboard_handler(struct InterruptFrame* frame)
{
    u8 scancode = inb(PS2_KEYBOARD_PORT);

    if (kb_event_count == UINT8_MAX)
    {
        overflow = true;
    }
    kb_buffer.messages[kb_event_count++] = scancode;

    PIC_end_master();
}

u8 mouse_cycle = 0;
u8 mouse_packet[4];
bool mouse_packet_ready = false;

void PS2_mouse_handle(u8 data)
{
    if (mouse_packet_ready)
    {
        return;
    }

    switch(mouse_cycle)
    {
        case 0:
            if ((data & 0b00001000) != 0)
            {
                mouse_packet[0] = data;
                mouse_cycle++;
            }
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            mouse_packet_ready = true;
            mouse_cycle = 0;
            break;
        default:
            break;
    }
}

INTERRUPT_HANDLER void mouse_handler(struct InterruptFrame* frame)
{
    u8 mouse_data = inb(0x60);

    PS2_mouse_handle(mouse_data);
    //print(unsigned_to_string(mouse_cycle));
    
    PIC_end_slave();
}

// Defined at the bottom of the file
extern const char US_QWERTY_ASCII_table[];
extern const char ES_QWERTY_ASCII_table[];
extern const char* ASCII_table;

char translate_scancode(u8 scancode, bool uppercase)
{
    return (ASCII_table[scancode] - (uppercase * 32)) * (scancode <= 58);
}

static void key_press(u8 scancode)
{
    KeyboardBitmap_set_value(&keymap, scancode, true);
}

static void key_release(u8 scancode)
{
    KeyboardBitmap_set_value(&keymap, scancode, false);
}

bool is_key_pressed(u8 scancode)
{
    return KeyboardBitmap_get_value(keymap, scancode);
}

void handle_keyboard(u8 scancode)
{
}

void PIC_end_master(void)
{
    outb(PIC1_COMMAND, PIC_EOI);
}

void PIC_end_slave(void)
{
    outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void PIC_remap(void)
{
    u8 a1 = inb(PIC1_DATA);
    io_wait();
    u8 a2 = inb(PIC2_DATA);
    io_wait();

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, a1);
    io_wait();
    outb(PIC2_DATA, a2);
}

const char US_QWERTY_ASCII_table[] =
{
    0,  
    0,  
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    0,  
    0,  
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    0,  
    0,  
    'a',
    's',
    'd',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    '`', 
    0,   
    '\\',
    'z', 
    'x', 
    'c', 
    'v', 
    'b', 
    'n', 
    'm', 
    ',', 
    '.', 
    '/', 
    0,   
    '*', 
    0,   
    ' ', 
};


const char ES_QWERTY_ASCII_table[] =
{
    0,      // 00
    0,      // 01
    '1',    // 02
    '2',    // 03
    '3',    // 04
    '4',    // 05
    '5',    // 06
    '6',    // 07
    '7',    // 08
    '8',    // 09
    '9',    // 10
    '0',    // 11
    '\'',    // 12
    '=',    // 13 // @tricky
    0,      // 14 // @tricky
    0,      // 15 // @tricky
    'q',    // 16
    'w',    // 17
    'e',    // 18
    'r',    // 19
    't',    // 20
    'y',    // 21
    'u',    // 22
    'i',    // 23
    'o',    // 24
    'p',    // 25
    '[',    // 26 // @tricky
    '+',    // 27
    0,      // 28
    0,      // 29
    'a',    // 30
    's',    // 31
    'd',    // 32
    'f',    // 33
    'g',    // 34
    'h',    // 35
    'j',    // 36
    'k',    // 37
    'l',    // 38
    'n',    // 39 // @tricky
    0,      // 40 // @tricky
    '\'',   // 41 
    '`',    // 42
    '<',    // 43
    'z',    // 44,
    'x',    // 45
    'c',    // 46
    'v',    // 47
    'b',    // 48
    'n',    // 49
    'm',    // 50
    ',',    // 51
    '.',    // 52
    '-',    // 53
    '/',    // 54 // @tricky
    0,      // 55
    '*',    // 56
    ' ',      // 57
    0,    // 58
};

const char* ASCII_table = ES_QWERTY_ASCII_table;

void PS2_mouse_wait(void)
{
    u64 timeout = 100000;
    while (timeout--)
    {
        if ((inb(0x64) & 0b10) == 0)
        {
            return;
        }
    }
}

void PS2_mouse_wait_input(void)
{
    u64 timeout = 100000;
    while (timeout--)
    {
        if (inb(0x64) & 0b1)
        {
            return;
        }
    }
}

void PS2_mouse_write(u8 value)
{
    PS2_mouse_wait();
    outb(0x64, 0xD4);
    PS2_mouse_wait();
    outb(0x60, value);
}

u8 PS2_mouse_read(void)
{
    PS2_mouse_wait_input();
    return inb(0x60);
}

void PS2_mouse_init(void)
{
    outb(0x64, 0xA8); // enabling the auxiliary device -mouse

    PS2_mouse_wait();
    outb(0x64, 0x20); // tells the keyboard controller that we want to send a command to the mouse
    PS2_mouse_wait_input();
    u8 status = inb(0x60);
    status |= 0b10;
    PS2_mouse_wait();
    outb(0x64, 0x60);
    PS2_mouse_wait();
    outb(0x60, status); // setting the correct bit is the "compaq" status byte

    PS2_mouse_write(0xF6);
    PS2_mouse_read();

    PS2_mouse_write(0xF4);
    PS2_mouse_read();
}
