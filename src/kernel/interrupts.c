#include "config.h"
#include "interrupts.h"
#include "asm.h"
#include "libk.h"
#include "panic.h"

extern void clear_char(void);
extern void* request_page(void);
extern void memmap(void *, void *);

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr30(void);
extern void isr33(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr39(void);

typedef struct PACKED InterruptStack
{
    u64 r15, r14, r13, r12, r11, r10, r9, r8, rsi, rdi, rbp, rdx, rcx, rbx, rax;
    u64 int_no, error_code;
    u64 rip, cs, rflags, rsp, ss;
} InterruptStack;

typedef struct PACKED GDTDescriptor
{
    u16 size;
    u64 offset;
} GDTDescriptor;

typedef struct PACKED GDTEntry
{
    u16 limit0;
    u16 base0;
    u8 base1;
    u8 access_byte;
    u8 limit1_flags;
    u8 base2;
} GDTEntry;

typedef struct PACKED ALIGN(0x1000) GDT
{
    GDTEntry kernel_null; // 0x00
    GDTEntry kernel_code; // 0x08
    GDTEntry kernel_data; // 0x10
    GDTEntry user_null;
    GDTEntry user_code;
    GDTEntry user_data;
} GDT;

static ALIGN(0x1000) GDT default_GDT = 
{
    .kernel_null = { 0, 0, 0, 0x00, 0x00, 0 },
    .kernel_code = { 0, 0, 0, 0x9a, 0xa0, 0 },
    .kernel_data = { 0, 0, 0, 0x92, 0xa0, 0 },
    .user_null =   { 0, 0, 0, 0x00, 0x00, 0 },
    .user_code =   { 0, 0, 0, 0x9a, 0xa0, 0 },
    .user_data =   { 0, 0, 0, 0x92, 0xa0, 0 },
};
extern void load_gdt(GDTDescriptor* gdt_descriptor);

typedef struct PACKED IDTDescriptor
{
    u16 offset0;
    u16 selector;
    u8 ist;
    u8 type_attribute;
    u16 offset1;
    u32 offset2;
    u32 reserved;
} IDTDescriptor;

typedef struct PACKED IDTRegister
{
    u16 limit;
    u64 address;
} IDTRegister;

enum
{
    IDT_TA_InterruptGate = 0b10001110,
    IDT_TA_CallGate = 0b10001100,
    IDT_TA_TrapGate = 0b10001111,
};

typedef void InterruptHandler(InterruptStack* is);
InterruptHandler* ISR[256];
void ISR_double_fault_handler(InterruptStack* stack);
void ISR_general_protection_fault_handler(InterruptStack* stack);
void ISR_page_fault_handler(InterruptStack* stack);
void ISR_keyboard_handler(InterruptStack* stack);
void ISR_mouse_handler(InterruptStack* stack);

static volatile const u64 IA32_APIC_base = 0x1b;
u64 LAPIC_address = 0;
u64 HPET_address = 0;
static u64 HPET_frequency = 1000000000000000;
static u64 HPET_clk = 0;

static IDTRegister IDT_register;
static IDTDescriptor* IDT_descriptors;

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
        println("%32u: %64h", i, (u64)frame_addresses[i]);
    }
}

void stacktrace_asm(u64* rbp, u32 max_frame_count)
{
    println("RBP %64h", rbp);
    println("Next RBP %64h", *rbp);
    println("Stacktrace\n==========");
    u32 i = 0;
    while (rbp)
    {
        u64 rip = *(rbp + 1);
        u64 next_rbp = *(rbp + 0);
        println("Call %32u: %64h", i++, rip);
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

    println("Wrong value for key: %64u", index);
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

void ISR_page_fault_handler(struct InterruptStack* stack)
{
    panic("Page fault detected");
    for(;;);
}

void ISR_double_fault_handler(struct InterruptStack* stack)
{
    panic("Double fault detected");
    for(;;);
}

void ISR_general_protection_fault_handler(struct InterruptStack* stack)
{
    panic("General protection fault detected. Interrupt number: %64u. Error code: %64u\n", stack->int_no, stack->error_code);
    u64* rbp;
    asm("movq %%rbp, %0;" : "=r"(rbp) ::);
    stacktrace_asm(rbp, 10);
    stacktrace_asm((u64*)stack->rbp, 10);
    
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

void ISR_keyboard_handler(struct InterruptStack* stack)
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

void ISR_mouse_handler(struct InterruptStack* stack)
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

void PIC_end_master(void)
{
    outb(PIC1_COMMAND, PIC_EOI);
}

void PIC_end_slave(void)
{
    outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void PIC_mask(void)
{
    outb(PIC1_DATA, 0xf9);
    outb(PIC2_DATA, 0xff);
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

enum
{
    IDT_TypePrivilege = 0,
    IDT_TypePresent = 1,
};

void IDT_gate_new(u8 index, u8 gate, void (*handler)(void))
{
    IDT_descriptors[index] = (const IDTDescriptor)
    {
        .offset0 = (u16)((u64)handler & 0x000000000000ffff),
        .selector = 1 * sizeof(GDTEntry),
        .ist = 0,
        .type_attribute = IDT_TypePresent << 7 | IDT_TypePrivilege << 5 | gate,
        .offset1 = (u16)(((u64)handler & 0x00000000ffff0000) >> 16),
        .offset2 = (u32)(((u64)handler & 0xffffffff00000000) >> 32),
        .reserved = 0,
    };
}

void IDT_load(void)
{
    asm("lidt %0" : : "m" (IDT_register));
}

void GDT_setup(void)
{
    load_gdt(&(GDTDescriptor) { .size = sizeof(GDT) - 1, .offset = (u64)&default_GDT, });
}

void interrupts_setup(void)
{
    usize IDT_size = 256 * sizeof(IDTDescriptor);
    IDT_register.limit = IDT_size - 1;
    IDT_register.address = (u64)request_page();
    IDT_descriptors = (IDTDescriptor*)IDT_register.address;
    memset(IDT_descriptors, 0, IDT_size);

    IDT_gate_new(8,  IDT_TA_TrapGate, isr8); // double fault
    IDT_gate_new(13, IDT_TA_TrapGate, isr13); // GP fault
    IDT_gate_new(14, IDT_TA_TrapGate, isr14); // page fault
    IDT_gate_new(33, IDT_TA_InterruptGate, isr33); // keyboard interrupt
    //IDT_gate_new(44, IDT_TA_InterruptGate, isr44); // mouse interrupt

    ISR[8] = ISR_double_fault_handler;
    ISR[13] = ISR_general_protection_fault_handler;
    ISR[14] = ISR_page_fault_handler;
    ISR[33] = ISR_keyboard_handler;
    //ISR[44] = ISR_mouse_handler;

#if APIC == 0
    PIC_remap();
    PIC_mask();
#endif
    IDT_load();
    interrupts_enable();
}

void PIC_mask_IRQ(u8 IRQ)
{
    u16 port;
    if (IRQ < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        IRQ -= 8;
    }

    u8 value = inb(port);
    value |= 1 << IRQ;
    outb(port, value);
}

void PIC_unmask_IRQ(u8 IRQ)
{
    u16 port;
    if (IRQ < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        IRQ -= 8;
    }

    u8 value = inb(port);
    value &= ~(1 << IRQ);
    outb(port, value);
}

void APIC_remap_PIC(void)
{
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
}

void APIC_PIC_mask_all(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void APIC_disable_PIC(void)
{
    APIC_remap_PIC();
    APIC_PIC_mask_all();
}

static const u32 APIC_LVT_INT_UNMASKED = 0 << 16;
static const u32 APIC_LVT_INT_MASKED = 1 << 16;
static const u32 APIC_LVT_DELIVERY_MODE_NMI = 4 << 8;
static const u32 APIC_SPURIOUS_IVT_SOFTWARE_ENABLE = 0x100;
static const u32 APIC_LVT_TIMER_MODE_PERIODIC = 1 << 17;

#define PIT_FREQUENCY	1193180

#define PIT_CHANNEL_0				0x00	// 00......
#define PIT_CHANNEL_1				0x40	// 01......
#define PIT_CHANNEL_2				0x80	// 10......
#define PIT_CHANNEL_READBACK		0xC0	// 11......
#define PIT_ACCESS_LATCHCOUNT		0x00	// ..00....
#define PIT_ACCESS_LOBYTE			0x10	// ..01....
#define PIT_ACCESS_HIBYTE			0x20	// ..10....
#define PIT_ACCESS_LOHIBYTE			0x30	// ..11....
#define PIT_OPMODE_0_IOTC			0x00	// ....000.
#define PIT_OPMODE_1_ONESHOT		0x02	// ....001.
#define PIT_OPMODE_2_RATE_GEN		0x04	// ....010.
#define PIT_OPMODE_3_SQUARE_WAV		0x06	// ....011.
#define PIT_OPMODE_4_SOFTWARESTROBE	0x08	// ....100.
#define PIT_OPMODE_4_HARDWARESTROBE	0x0A	// ....101.
#define PIT_OPMODE_4_RATE_GEN		0x0C	// ....110.
#define PIT_OPMODE_4_SQUARE_WAV		0x0E	// ....111.
#define PIT_BINARY					0x00	// .......0
#define PIT_BCD						0x01	// .......1

static u32 sleep_divisor;

void PIT_prepare_sleep(u32 microseconds)
{
    if (microseconds > 54000)
    {
        panic("Illegal use of sleep");
        return;
    }

    u8 speaker_control_byte = inb(0x61);
    speaker_control_byte &= ~2;
    outb(0x61, speaker_control_byte);

    outb(0x43, PIT_CHANNEL_2 | PIT_OPMODE_0_IOTC | PIT_ACCESS_LOHIBYTE);

    sleep_divisor = PIT_FREQUENCY / (1000 * 1000 / microseconds);
}

void PIT_sleep(void)
{
    outb(0x42, sleep_divisor & 0xFF);
    outb(0x42, sleep_divisor >> 8);

    u8 PIT_control_byte = inb(0x61);
    outb(0x61, (u8) PIT_control_byte & ~1);
    outb(0x61, (u8) PIT_control_byte | 1);

    while (!(inb(0x61) & 0x20));
}



typedef enum LAPICRegister
{
    LAPIC_Reserved0 = 0x000,
    LAPIC_ID_Register = 0x020,
    LAPIC_VersionRegister = 0x030,
    LAPIC_Reserved1 = 0x040,
    TaskPriorityRegister = 0x080,
    ArbitrationPriorityRegister = 0x090,
    ProcessorPriorityRegister = 0x0A0,
    EOI_Register = 0x0B0,
    RemoteReadRegister = 0x0C0,
    LogicalDestinationRegister = 0x0D0,
    DestinationFormatRegister = 0x0E0,
    SpuriousInterruptVectorRegister = 0x0F0,
    In_ServiceRegister = 0x100,
    TriggerModeRegister = 0x180,
    InterruptRequestRegister = 0x200,
    ErrorStatusRegister = 0x280,
    LAPIC_Reserved2 = 0x290,
    LVT_CorrectedMachineCheckInterruptRegister = 0x2F0,
    InterruptCommandRegister = 0x300,
    LVT_TimerRegister = 0x320,
    LVT_ThermalSensorRegister = 0x330,
    LVT_PerformanceMonitoringCounterRegister = 0x340,
    LVT_LINT0_Register = 0x350,
    LVT_LINT1_Register = 0x360,
    LVT_ErrorRegister = 0x370,
    InitialCountRegisterForTimer = 0x380,
    CurrentCountRegisterForTimer = 0x390,
    LAPIC_Reserved3 = 0x3A0,
    DivideConfigurationRegisterForTimer = 0x3E0,
    LAPIC_Reserved4 = 0x3F0,
} LAPICRegister;

void LAPIC_write(u16 offset, u32 value)
{
    u32* volatile lapic_address = (u32* volatile) (LAPIC_address + offset);
    *lapic_address = value;
}

u32 LAPIC_read(u16 offset)
{
    u32* volatile lapic_address = (u32* volatile) (LAPIC_address + offset);
    return *lapic_address;
}

bool APIC_enabled(void)
{
    return rdmsr(IA32_APIC_base) & (1 << 11);
}

void LAPIC_setup(void)
{
    memmap((void*)LAPIC_address, (void*)LAPIC_address);

    LAPIC_write(DestinationFormatRegister, 0xFFFFFFFF);
    u32 ldr = LAPIC_read(LogicalDestinationRegister);
    LAPIC_write(LogicalDestinationRegister, (ldr & 0x00FFFFFF) | 1);
    LAPIC_write(LVT_TimerRegister, APIC_LVT_INT_MASKED);
    LAPIC_write(LVT_PerformanceMonitoringCounterRegister, APIC_LVT_DELIVERY_MODE_NMI);
    LAPIC_write(LVT_LINT0_Register, APIC_LVT_INT_MASKED);
    LAPIC_write(LVT_LINT1_Register, APIC_LVT_INT_MASKED);
    LAPIC_write(TaskPriorityRegister, 0);
    LAPIC_write(SpuriousInterruptVectorRegister, 0xFF | APIC_SPURIOUS_IVT_SOFTWARE_ENABLE);

    if (APIC_enabled())
    {
        println("LAPIC initialized successfully!");
    }
    else
    {
        panic("LAPIC failed to initialize!");
    }
}

void HPET_write(u64 reg, u64 value)
{
    u64* volatile hpet_address = (u64* volatile) (HPET_address + reg);
    *hpet_address = value;
}

u64 HPET_read(u64 reg)
{
    u64* volatile hpet_address = (u64* volatile) (HPET_address + reg);
    return *hpet_address;
}

typedef enum HPETRegister
{
    GeneralCapabilitiesAndIDRegister = 0x000,
    GeneralConfigurationRegister = 0x010,
    GeneralInterruptStatusRegister = 0x020,
    MainCounterValueRegister = 0x0F0,
} HPETRegister;

void HPET_setup(void)
{
    memmap((void*)HPET_address, (void*)HPET_address);
    HPET_write(GeneralConfigurationRegister, 1);
    u64 cnf_reg = HPET_read(GeneralConfigurationRegister);
    u64 period_helper = HPET_read(GeneralCapabilitiesAndIDRegister);
    HPET_clk = (period_helper >> 32) & 0xFFFFFFFF;
    HPET_frequency /= HPET_clk;

    if (((cnf_reg & 1) == 1))
    {
        println("HPET initialized! Frequency: %64u femtoseconds", HPET_frequency);
    }
    else
    {
        panic("HPET failed to initialize!");
    }
}

static void LAPIC_timer_set_mask(u64 mask)
{
    u32 entry = LAPIC_read(LVT_TimerRegister);
    if (mask)
    {
        entry |= 1UL << 16;
    }
    else
    {
        entry &= ~(1UL << 16);
    }

    LAPIC_write(LVT_TimerRegister, entry);
}

void HPET_poll_and_sleep(u64 ms)
{
    u64 main_counter_value = HPET_read(MainCounterValueRegister);
    u64 target = main_counter_value + (ms * 1000000000000) / HPET_clk;

    while (main_counter_value < target)
    {
        main_counter_value = HPET_read(MainCounterValueRegister);
    }
}

void LAPIC_timer_setup(void)
{
    LAPIC_write(DivideConfigurationRegisterForTimer, 0x3);

    LAPIC_write(InitialCountRegisterForTimer, 0xffffffff);
    HPET_poll_and_sleep(10);
    LAPIC_write(LVT_TimerRegister, APIC_LVT_INT_MASKED);

    u64 tick_count = LAPIC_read(CurrentCountRegisterForTimer);
    u64 ticks_per_ms = (0xffffffff - tick_count) / 10;

    LAPIC_write(LVT_TimerRegister, 32 | APIC_LVT_TIMER_MODE_PERIODIC);
    LAPIC_write(DivideConfigurationRegisterForTimer, 0x3);
    //LAPIC_write(InitialCountRegisterForTimer, ticks_per_ms);

    println("LAPIC timer initialized! Tick count: %64u", tick_count);
}

void APIC_IA32_base_setup(void)
{
    wrmsr(IA32_APIC_base, ~(1 << 10));
    wrmsr(IA32_APIC_base, (1 << 11));
    
    asm volatile("mov %0, %%cr8" :: "r"(0ULL));
}

void APIC_setup(void)
{
    APIC_disable_PIC();
    APIC_IA32_base_setup();
    LAPIC_setup();
    HPET_setup();
    LAPIC_timer_setup();
}