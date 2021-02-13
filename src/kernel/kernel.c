#include "types.h"
#include "asm.h"
#include "interrupts.h"

u64 i = 0;

typedef struct Framebuffer
{
    void* base_address;
    usize size; u32 width, height;
    u32 pixels_per_scanline;
} Framebuffer;

// @TODO: first bit can cause problems, check out
typedef enum Color
{
    Color_Black     = 0xff000000,
    Color_Red       = 0x00ff0000,
    Color_Green     = 0x0000ff00,
    Color_Blue      = 0x000000ff,
    Color_White     = Color_Blue | Color_Green | Color_Red,
} Color;

typedef struct PSF1Header
{
    u8 magic[2];
    u8 mode;
    u8 char_size;
} PSF1Header;

typedef struct PSF1Font
{
    PSF1Header* header;
    void* glyph_buffer;
} PSF1Font;

typedef struct Point
{
    s64 x, y;
} Point;

typedef struct Renderer
{
    Framebuffer* fb;
    PSF1Font* font;
    Point cursor_position;
    Color color;
    Color clear_color;
} Renderer;

typedef struct EfiMemoryDescriptor
{
    u32 type;
    void* physical_address;
    void* virtual_address;
    u64 page_count;
    u64 attributes;
} EfiMemoryDescriptor;

typedef struct EFIMmap
{
    EfiMemoryDescriptor* handle;
    u64 size;
    u64 descriptor_size;
} EFIMmap;

typedef struct PACKED ACPI_SDT_Header
{
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char OEM_ID[6];
    char OEM_table_ID[8];
    u32 OEM_revision;
    u32 creator_ID;
    u32 creator_revision;
} ACPI_SDT_Header;

typedef struct PACKED ACPI_MCFG_Header
{
    ACPI_SDT_Header header;
    u64 reserved;
} ACPI_MCFG_Header;

typedef struct PACKED ACPI_MADT_Header
{
    ACPI_SDT_Header header;
    u32 LAPIC_address;
    u32 flags;
} ACPI_MADT_Header;

typedef struct PACKED ACPI_HPET_Header
{
    ACPI_SDT_Header header;
    u8 hardware_revision_ID;
    u8 comparator_count:5;
    u8 counter_size:1;
    u8 reserved0:1;
    u8 legacy_replacement:1;
    u16 PCI_vendor_ID;
    u8 address_space_ID;
    u8 register_bit_width;
    u8 register_bit_offset;
    u8 reserved1;
    u64 address;
    u8 HPET_number;
    u16 minimum_tick;
    u8 page_protection;
} ACPI_HPET_Header;

typedef struct PACKED HPET_GeneralCapabilitiesAnd_ID_Register
{
    u64 revision_ID:8;
    u64 num_time_cap:5;
    u64 count_size_cap:1;
    u64 reserved:1;
    u64 leg_rt_cap:1;
    u64 vendor_ID:16;
    u64 counter_clk_period:32;
} HPET_GeneralCapabilitiesAnd_ID_Register;

typedef struct PACKED HPET_GeneralConfigurationRegister
{
    u64 enable_cnf:1;
    u64 leg_rt_cnf:1;
    u64 reserved:62;
} HPET_GeneralConfigurationRegister;

typedef struct PACKED HPET_GeneralInterruptStatusRegister
{
    u32 n; // runtime-dependent
    u32 reserved;
} HPET_GeneralInterruptStatusRegister;

typedef struct PACKED HPET_MainCounterValueRegister
{
    u64 main_counter_val;
} HPET_MainCounterValueRegister;

typedef struct PACKED HPET_TimerNConfigurationAndCapabilityRegister
{
    u64 reserved0:1;
    u64 int_type_cnf:1;
    u64 int_enb_conf:1;
    u64 type_cnf:1;
    u64 per_int_cap:1;
    u64 size_cap:1;
    u64 val_set_cnf:1;
    u64 reserved:1;
    u64 mode32_cnf:1;
    u64 int_route_cnf:5;
    u64 fsb_en_cnf:1;
    u64 fst_int_del_cap:1;
    u64 reserved1:16;
    u64 int_route_cap:32;

} HPET_TimerNConfigurationAndCapabilityRegister;

typedef struct PACKED HPETRegister
{
    HPET_GeneralCapabilitiesAnd_ID_Register cap_and_ID;
    u64 pad0;
    HPET_GeneralConfigurationRegister configuration;
    u64 pad1;
    HPET_GeneralInterruptStatusRegister interrupt_status;
    u64 pad2;
    u64 padding[12 * 2];
    HPET_MainCounterValueRegister main_counter_value;
} HPETRegisters;

typedef struct PACKED ACPI_MADT_EntryHeader
{
    u8 entry_type;
    u8 record_length;
} ACPI_MADT_EntryHeader;

typedef enum ACPI_MADT_EntryType
{
    MADT_EntryType_LAPIC = 0,
    MADT_EntryType_IO_APIC = 1,
    MADT_EntryType_InterruptSourceOverride = 2,
    MADT_EntryType_NonMaskableInterrupts = 4,
    MADT_EntryType_LAPIC_AddressOverride = 5,
    MADT_EntryType_Count = MADT_EntryType_LAPIC_AddressOverride,
} ACPI_MADT_EntryType;

typedef struct PACKED ACPI_MADT_LAPIC_Entry
{
    ACPI_MADT_EntryHeader header;
    u8 ACPI_processor_ID;
    u8 APIC_ID;
    u32 flags;
} ACPI_MADT_LAPIC_Entry;

typedef struct PACKED ACPI_MADT_IO_APIC_Entry
{
    ACPI_MADT_EntryHeader header;
    u8 IO_APIC_ID;
    u8 reserved;
    u32 IO_APIC_address;
    u32 global_system_interrupt_base;
} ACPI_MADT_IO_APIC_Entry;

typedef struct PACKED ACPI_MADT_InterruptSourceOverride_Entry
{
    ACPI_MADT_EntryHeader header;
    u8 bus_source;
    u8 IRQ_source;
    u32 global_system_interrupt;
    u16 flags;
} ACPI_MADT_InterruptSourceOverride_Entry;

typedef struct PACKED ACPI_MADT_NonMaskableInterrupts_Entry
{
    ACPI_MADT_EntryHeader header;
    u8 ACPI_processor_ID;
    u16 flags;
    u8 LINT;
} ACPI_MADT_NonMaskableInterrupts_Entry;

typedef struct PACKED ACPI_MADT_LAPIC_AddressOverride_Entry
{
    ACPI_MADT_EntryHeader header;
    u16 reserved;
    u64 LAPIC_physical_address;
} ACPI_MADT_LAPIC_AddressOverride_Entry;

typedef struct PACKED LAPIC_Register
{
    u32 reg;
    u32 padding[3];
} LAPIC_Register;

typedef struct PACKED ICR
{
    u32 vector_number:8;
    u32 destination_mode:3;
    u32 destination_mode1:1;
    u32 delivery_status:1;
    u32 reserved0:1;
    u32 bit14:1;
    u32 bit15:1;
    u32 reserved2:2;
    u32 destination_type:2;
    u32 reserved1:12;
} ICR;

typedef struct PACKED LAPICRegisters
{
    LAPIC_Register reserved0[2];
    LAPIC_Register ID;
    LAPIC_Register version;
    LAPIC_Register reserved1[4];
    LAPIC_Register TPR;
    LAPIC_Register APR;
    LAPIC_Register PPR;
    LAPIC_Register EOI;
    LAPIC_Register RRD;
    LAPIC_Register LDR;
    LAPIC_Register DFR;
    LAPIC_Register SIV;
    LAPIC_Register ISR[8];
    LAPIC_Register TMR[8];
    LAPIC_Register IRR[8];
    LAPIC_Register ESR;
    LAPIC_Register reserved2[6];
    LAPIC_Register LVT_CMCI;
    LAPIC_Register ICR[2];
    LAPIC_Register LVT_Timer;
    LAPIC_Register LVT_ThermalSensor;
    LAPIC_Register LVT_PMC;
    LAPIC_Register LVT_LINT0;
    LAPIC_Register LVT_LINT1;
    LAPIC_Register LVT_Error;
    LAPIC_Register ICR_timer;
    LAPIC_Register CCR_timer;
    LAPIC_Register reserved3[4];
    LAPIC_Register DCR_timer;
    LAPIC_Register reserved4;
} LAPICRegisters;

typedef struct PACKED IO_APIC_ID_Register
{
    u32 reserved0:24;
    u32 id:4;
    u32 reserved1:4;
} IO_APIC_ID_Register;

typedef struct PACKED IO_APIC_VersionRegister
{
    u32 version:8;
    u32 reserved0:8;
    u32 max_redirection_entries:8;
    u32 reserved:8;
} IO_APIC_VersionRegister;

typedef struct PACKED IO_APIC_ArbitrationPriorityRegister
{
    u32 reserved0:24;
    u32 arbitration_priority:4;
    u32 reserved1:4;
} IO_APIC_ArbitrationPriorityRegister;

typedef struct PACKED IO_APIC_RedirectionEntry
{
    u64 interrupt_vector:8;
    u64 delivery_mode:3;
    u64 destination_mode:1;
    u64 delivery_status:1;
    u64 pin_polarity:1;
    u64 remote_IRR:1;
    u64 trigger_mode:1;
    u64 mask:1;
    u64 unknown:39;
    u64 destination:8;
} IO_APIC_RedirectionEntry;

typedef struct PACKED IO_APIC
{
    IO_APIC_ID_Register id;
    IO_APIC_VersionRegister version;
    IO_APIC_ArbitrationPriorityRegister arbitration_priority;
    u32 padding[13];

} IO_APIC;

typedef struct PACKED ACPI_RSDPDescriptor1
{
    char signature[8];
    u8 checksum;
    char OEM_ID[6];
    u8 revision;
    u32 RSDT_address;
} ACPI_RSDPDescriptor1;

typedef struct PACKED ACPI_RSDPDescriptor2
{
    ACPI_RSDPDescriptor1 descriptor1;
    u32 length;
    u64 XSDT_address;
    u8 extended_checksum;
    u8 reserved[3];
} ACPI_RSDPDescriptor2;

typedef struct PACKED ACPI_DeviceConfig
{
    u64 base_address;
    u16 PCI_seg_group;
    u8 start_bus;
    u8 end_bus;
    u32 reserved;
} ACPI_DeviceConfig;

typedef struct PACKED PCI_DeviceHeader
{
    u16 vendor_ID;
    u16 device_ID;
    u16 command;
    u16 status;
    u8 revision_ID;
    u8 program_interface;
    u8 subclass;
    u8 class;
    u8 cache_line_size;
    u8 latency_timer;
    u8 header_type;
    u8 BIST;
} PCI_DeviceHeader;

typedef struct BootInfo
{
    Framebuffer* framebuffer;
    PSF1Font* font;
    EFIMmap mmap;
    ACPI_RSDPDescriptor2* rsdp;
} BootInfo;

typedef enum EFIMemoryType
{
    EfiReservedMemoryType = 0,
    EfiLoaderCode = 1,
    EfiLoaderData = 2,
    EfiBootServicesCode = 3,
    EfiBootServicesData = 4,
    EfiRuntimeServicesCode = 5,
    EfiRuntimeServicesData = 6,
    EfiConventionalMemory = 7,
    EfiUnusableMemory = 8,
    EfiACPIReclaimMemory = 9,
    EfiACPIMemoryNVS = 10,
    EfiMemoryMappedIO = 11,
    EfiMemoryMappedIOPortSpace = 12,
    EfiPalCode = 13,
} EFIMemoryType;

const char* EFI_memory_type_strings[] =
{
    "EfiReservedMemoryType",
    "EfiLoaderCode",
    "EfiLoaderData",
    "EfiBootServicesCode",
    "EfiBootServicesData",
    "EfiRuntimeServicesCode",
    "EfiRuntimeServicesData",
    "EfiConventionalMemory",
    "EfiUnusableMemory",
    "EfiACPIReclaimMemory",
    "EfiACPIMemoryNVS",
    "EfiMemoryMappedIO",
    "EfiMemoryMappedIOPortSpace",
    "EfiPalCode",
};

typedef struct BitMap
{
    usize size;
    u8* buffer;
} BitMap;

typedef enum PDEBit
{
    PDEBit_Present = 0,
    PDEBit_ReadWrite = 1,
    PDEBit_UserSuper = 2,
    PDEBit_WriteThrough = 3,
    PDEBit_CacheDisabled = 4,
    PDEBit_Accessed = 5,
    PDEBit_LargerPages = 7,
    PDEBit_Custom0 = 9,
    PDEBit_Custom1 = 10,
    PDEBit_Custom2 = 11,
    PDEBit_NX = 63, // @Info: only supported in some systems
} PDEBit;

typedef u64 PageDirectoryEntry;

typedef struct ALIGN(0x1000) PageTable 
{
    PageDirectoryEntry entries[512];
} PageTable;

typedef struct PageMapIndexer
{
    u64 PDP_i;
    u64 PD_i;
    u64 PT_i;
    u64 P_i;
} PageMapIndexer;

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

typedef struct IDTDescriptor
{
    u16 offset0;
    u16 selector;
    u8 ist;
    u8 type_attribute;
    u16 offset1;
    u32 offset2;
    u32 reserved;
} IDTDescriptor;

typedef struct PACKED IDTR
{
    u16 limit;
    u64 offset;
} IDTR;

enum
{
    IDT_TA_InterruptGate = 0b10001110,
    IDT_TA_CallGate = 0b10001100,
    IDT_TA_TrapGate = 0b10001111,
};

typedef struct PACKED TerminalCommandBuffer
{
    char characters[1022];
    s16 char_count;
} TerminalCommandBuffer;

typedef char CommandArg[96];

typedef struct Command
{
    CommandArg name;
    CommandArg args[9];
} Command;

typedef enum FormatLookupTableIndex
{
    BINARY = 0,
    DECIMAL = 1,
    HEXADECIMAL = 2,
} FormatLookupTableIndex;

typedef void KernelCommandFn(Command* cmd);

typedef struct KernelCommand
{
    const char* name;
    KernelCommandFn* dispatcher;
    u8 min_args;
    u8 max_args;
} KernelCommand;


extern u64 _KernelStart;
extern u64 _KernelEnd;
static u64 kernel_size;
static u64 kernel_page_count;


static ALIGN(0x1000) GDT default_GDT = 
{
    .kernel_null = { 0, 0, 0, 0x00, 0x00, 0 },
    .kernel_code = { 0, 0, 0, 0x9a, 0xa0, 0 },
    .kernel_data = { 0, 0, 0, 0x92, 0xa0, 0 },
    .user_null =   { 0, 0, 0, 0x00, 0x00, 0 },
    .user_code =   { 0, 0, 0, 0x9a, 0xa0, 0 },
    .user_data =   { 0, 0, 0, 0x92, 0xa0, 0 },
};

static IDTR idtr;

static Renderer renderer = {0};

static char signed_to_string_output[128];
static char unsigned_to_string_output[128];
static char float_to_string_output[128];
static char hex_to_string_output[128];

static PageTable* PML4; 
static BitMap page_map;
static u64 free_memory;
static u64 reserved_memory;
static u64 used_memory;
static u64 last_page_map_index = 0;

static Point mouse_position;
static Point old_mouse_position;
static u8 mouse_pointer[] =
{
    0b11111111, 0b11100000,
    0b11111111, 0b10000000,
    0b11111110, 0b00000000,
    0b11111100, 0b00000000,
    0b11111000, 0b00000000,
    0b11110000, 0b00000000,
    0b11100000, 0b00000000,
    0b11000000, 0b00000000,
    0b11000000, 0b00000000,
    0b10000000, 0b00000000,
    0b10000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
};
static u32 mouse_cursor_buffer[array_length(mouse_pointer) * array_length(mouse_pointer)];
static u32 mouse_cursor_buffer_post_render[array_length(mouse_pointer) * array_length(mouse_pointer)];
static bool mouse_never_drawn = true;

static TerminalCommandBuffer cmd_buffer[8];
static u8 current_command = 0;
static bool allow_keyboard_input = true;
static bool left_shift_pressed = false;
static bool right_shift_pressed = false;

static char hex_lookup_table[255] =
{
    ['0'] = 0,
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
    ['a'] = 0xa,
    ['b'] = 0xb,
    ['c'] = 0xc,
    ['d'] = 0xd,
    ['e'] = 0xe,
    ['f'] = 0xf,
    ['A'] = 0xA,
    ['B'] = 0xB,
    ['C'] = 0xC,
    ['D'] = 0xD,
    ['E'] = 0xE,
    ['F'] = 0xF,
};

static char decimal_lookup_table[255] =
{
    ['0'] = 0,
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
};

static char binary_lookup_table[255] =
{
    ['0'] = 0,
    ['1'] = 1,
};

static const char* format_lookup_table[] =
{
    [BINARY] = binary_lookup_table,
    [DECIMAL] = decimal_lookup_table,
    [HEXADECIMAL] = hex_lookup_table
};

static const u64 format_values_per_digit[] = 
{
    [BINARY] = 2,
    [DECIMAL] = 10,
    [HEXADECIMAL] = 16,
};

void cmd_memdump(Command* cmd);
void cmd_ls(Command* cmd);
static const KernelCommand kernel_commands[] =
{
    [0] =
    {
        .name = "memdump",
        .dispatcher = cmd_memdump,
        .min_args = 2,
        .max_args = 2,
    },
    [1] = 
    {
        .name = "ls",
        .dispatcher = cmd_ls,
        .min_args = 0,
        .max_args = 255,
    },
};

static volatile LAPICRegisters* LAPIC = NULL;
static volatile HPETRegisters* HPET = NULL;
static u64 HPET_frequency = 1000000000000000;

extern void load_gdt(GDTDescriptor* gdt_descriptor);

static inline u64 abs(s64 value)
{
    if (value < 0)
    {
        value = -value;
    }

    return (u64)value;
}

void PDE_set_bit(PageDirectoryEntry* PDE, PDEBit bit, bool enabled)
{
    u64 bit_mask = (u64)1 << bit;
    *PDE &= ~bit_mask;
    *PDE |= bit_mask * enabled;
}

bool PDE_get_bit(PageDirectoryEntry PDE, PDEBit bit)
{
    u64 bit_mask = (u64)1 << bit;
    return PDE & bit_mask;
}
u64 PDE_get_address(PageDirectoryEntry PDE)
{
    return (PDE & 0x000ffffffffff000) >> 12;
}

void PDE_set_address(PageDirectoryEntry* PDE, u64 address)
{
    address &= 0x000000ffffffffff;
    *PDE &= 0xfff0000000000fff;
    *PDE |= address << 12;
}

u8 get_bit(BitMap bm, u64 index)
{
    if (index > bm.size * 8)
    {
        return 0;
    }

    u64 byte_index = index / 8;
    u8 bit_index = index % 8;
    u8 bit_indexer = 0b10000000 >> bit_index;
    
    return (bm.buffer[byte_index] & bit_indexer) > 0;
}

bool set_bit(BitMap bm, u64 index, bool value)
{
    if (index > bm.size * 8)
    {
        return false;
    }

    u64 byte_index = index / 8;
    u8 bit_index = index % 8;
    u8 bit_indexer = 0b10000000 >> bit_index;

    bm.buffer[byte_index] &= ~bit_indexer;
    if (value)
    {
        bm.buffer[byte_index] |= bit_indexer;
    }

    return true;
}

// @TODO: this is just 8-bit memset
void memset(void* address, u8 value, u64 bytes)
{
    u8* it = (u8*)address;
    for (u64 i = 0; i < bytes; i++)
    {
        it[i] = value;
    }
}

bool memequal(const void* a, const void* b, usize bytes)
{
    u8* a1 = (u8*)a;
    u8* b1 = (u8*)b;
    for (usize i = 0; i < bytes; i++)
    {
        if (a1[i] != b1[i])
        {
            return false;
        }
    }
    return true;
}

void* memcpy(void* dst, const void* src, usize bytes)
{
    u8* writer = dst;
    u8* reader = (u8*)src;
    for (u64 i = 0; i < bytes; i++)
    {
        *writer++ = *reader++;
    }

    return dst;
}

const char* unsigned_to_string(u64 value)
{
    u8 digit_count;
    u64 it = value;
    
    while (it / 10 > 0)
    {
        it /= 10;
        digit_count++;
    }

    u8 index = 0;

    while (value / 10 > 0)
    {
        u8 remainder = value % 10;
        value /= 10;
        unsigned_to_string_output[digit_count - index] = remainder + '0';
        index++;
    }

    u8 remainder = value % 10;
    unsigned_to_string_output[digit_count - index] = remainder + '0';
    unsigned_to_string_output[digit_count + 1] = 0;

    return unsigned_to_string_output;
}

const char* signed_to_string(s64 value)
{
    u8 is_negative = value < 0;
    if (is_negative)
    {
        value *= is_negative ? -1 : 1;
        signed_to_string_output[0] = '-';
    }

    u8 digit_count;
    u64 it = value;
    
    while (it / 10 > 0)
    {
        it /= 10;
        digit_count++;
    }

    u8 index = 0;

    while (value / 10 > 0)
    {
        u8 remainder = value % 10;
        value /= 10;
        signed_to_string_output[is_negative + digit_count - index] = remainder + '0';
        index++;
    }

    u8 remainder = value % 10;
    signed_to_string_output[is_negative + digit_count - index] = remainder + '0';
    signed_to_string_output[is_negative + digit_count + 1] = 0;

    return signed_to_string_output;
}

const char* float_to_string(f64 value, u8 decimal_digits)
{
    if (decimal_digits > 20)
    {
        decimal_digits = 20;
    }

    char* int_ptr = (char*)signed_to_string((s64)value);
    char* float_ptr = float_to_string_output;

    if (value < 0)
    {
        value *= -1;
    }


    while (*int_ptr)
    {
        *float_ptr++ = *int_ptr++;
    }

    *float_ptr++ = '.';

    f64 new_value = value - (s64)value;

    for (u8 i = 0; i < decimal_digits; i++)
    {
        new_value *= 10;
        *float_ptr++ = (s64)new_value + '0';
        new_value -= (s64)new_value;
    }
    *float_ptr = 0;

    return float_to_string_output;
}

const char* hex_to_string_bytes(u64 value, u8 bytes_to_print)
{
    u8 digits = bytes_to_print * 2 - 1;
    
    for (u8 i = 0; i < digits; i++)
    {
        u8* ptr = ((u8*)&value + i);

        u8 tmp = ((*ptr & 0xF0) >> 4);
        hex_to_string_output[(digits - (i * 2 + 1))] = tmp + (tmp > 9 ? 55 : '0');

        tmp = (*ptr) & 0x0F;
        hex_to_string_output[(digits - (i * 2))] = tmp + (tmp > 9 ? 55 : '0');
    }

    hex_to_string_output[(digits + 1)] = 0;


    return hex_to_string_output;
}

const char* hex_to_string(u64 value)
{
    u8 bytes_to_print;
    if (value <= UINT8_MAX)
    {
        bytes_to_print = sizeof(u8);
    }
    else if (value <= UINT16_MAX)
    {
        bytes_to_print = sizeof(u16);
    }
    else if (value <= UINT32_MAX)
    {
        bytes_to_print = sizeof(u32);
    }
    else if (value <= UINT64_MAX)
    {
        bytes_to_print = sizeof(u64);
    }

    return hex_to_string_bytes(value, bytes_to_print);
}

const char* hex_to_string_u8(u8 value)
{
    return hex_to_string_bytes(value, sizeof(u8));
}
const char* hex_to_string_u16(u16 value)
{
    return hex_to_string_bytes(value, sizeof(u16));
}
const char* hex_to_string_u32(u32 value)
{
    return hex_to_string_bytes(value, sizeof(u32));
}
const char* hex_to_string_u64(u64 value)
{
    return hex_to_string_bytes(value, sizeof(u64));
}

void print(const char*);
void println(const char*);
void print_hex(u64 value)
{
    print("0x");
    print(hex_to_string(value));
}
void println_hex(u64 value)
{
    print("0x");
    println(hex_to_string(value));
}

void scroll(Renderer* renderer)
{
    Framebuffer* fb = renderer->fb;

    u64 fb_base = (u64)fb->base_address;
    u64 bytes_per_scanline = fb->pixels_per_scanline * sizeof(Color);
    u64 fb_height = fb->height;
    u64 fb_size = fb->size;

    u8 char_size = renderer->font->header->char_size;
    u64 lines_to_be_skipped = char_size;
    u64 lines_to_be_copied = fb_height - lines_to_be_skipped;
    memcpy((void*)fb_base, (void*)(fb_base + (bytes_per_scanline * lines_to_be_skipped)), bytes_per_scanline * lines_to_be_copied);

    u32* line_clear_it = (u32*)(fb_base + (bytes_per_scanline * lines_to_be_copied));
    u32* top = (u32*)(fb_base + (bytes_per_scanline * fb_height));
    Color clear_color = renderer->clear_color;

    while (line_clear_it < top)
    {
        *line_clear_it++ = clear_color;
    }

    // Don't advance line, we are scrolling
    renderer->cursor_position.x = 0;
}

void new_line_ex(Renderer* renderer)
{
    u8 char_size = renderer->font->header->char_size;
    usize fb_height = renderer->fb->height;
    u32 space_to_obviate = char_size + (fb_height % char_size);
    u32 line_limit = fb_height - space_to_obviate;

    if (renderer->cursor_position.y + char_size < line_limit)
    {
        renderer->cursor_position = (Point) { .x = 0, .y = renderer->cursor_position.y + char_size };
    }
    else
    {
        scroll(renderer);
    }
}

void new_line(void)
{
    new_line_ex(&renderer);
}

void handle_newline_while_printing(Renderer* renderer)
{
    renderer->cursor_position.x += 8;
    if (renderer->cursor_position.x + 8 > renderer->fb->width)
    {
        new_line_ex(renderer);
    }
}

void render_char(Renderer* renderer, char c, u32 xo, u32 yo)
{
    u32* pix_writer = (u32*)renderer->fb->base_address;
    char* font_reader = renderer->font->glyph_buffer + (c * renderer->font->header->char_size);

    for (u64 y = yo; y < yo + 16; y++, font_reader++)
    {
        for (u64 x = xo; x < xo + 8; x++)
        {
            if ((*font_reader & (0b10000000 >> (x - xo))) > 0)
            {
                *(u32*)(pix_writer + x + (y * renderer->fb->pixels_per_scanline)) = renderer->color;
            }
        }
    }
}

void print_ex(Renderer* renderer, const char* str)
{
    if (str)
    {
        while (*str)
        {
            render_char(renderer, *str, renderer->cursor_position.x, renderer->cursor_position.y);
            handle_newline_while_printing(renderer);
            str++;
        }
    }
    else
    {
        print_ex(renderer, "(null)");
    }
}

void putc(char c)
{
    render_char(&renderer, c, renderer.cursor_position.x, renderer.cursor_position.y);
    handle_newline_while_printing(&renderer);
}

const char* unsigned_to_string(u64);

void put_pixel(s64 x, s64 y, Color color)
{
    *(u32*)( (u64)renderer.fb->base_address + (x*4) + (y * renderer.fb->pixels_per_scanline * 4)) = color;
}

u32 get_pixel(s64 x, s64 y)
{
    return *(u32*)( (u64)renderer.fb->base_address + (x*4) + (y * renderer.fb->pixels_per_scanline * 4));
}

void put_char_in_point(char c, u32 xo, u32 yo)
{
    render_char(&renderer, c, xo, yo);
}

void print(const char* str)
{
    print_ex(&renderer, str);
}


void println(const char* str)
{
    print(str);
    new_line();
}


void fb_clear(void)
{
    Framebuffer* fb = renderer.fb;
    u64 fb_base = (u64)fb->base_address;
    u64 bytes_per_scanline = fb->pixels_per_scanline * sizeof(Color);
    u64 fb_height = fb->height;
    u64 fb_size = fb->size;

    for (u64 vertical_scanline = 0; vertical_scanline < fb_height; vertical_scanline++)
    {
        u64 pix_base_ptr = fb_base + (bytes_per_scanline * vertical_scanline);
        u32* top_ptr = (u32*)(pix_base_ptr + bytes_per_scanline);

        for (u32* pix_ptr = (u32*)pix_base_ptr; pix_ptr < top_ptr; pix_ptr++)
        {
            *pix_ptr = renderer.clear_color;
        }
    }
}


void clear_char(void)
{
    if (renderer.cursor_position.x == 0)
    {
        renderer.cursor_position.x = renderer.fb->width;
        renderer.cursor_position.y -= 16;
        if (renderer.cursor_position.y < 0)
        {
            renderer.cursor_position.y = 0;
        }
    }

    u32* pix_writer = (u32*)renderer.fb->base_address;

    for (u64 y = renderer.cursor_position.y; y < renderer.cursor_position.y + 16; y++)
    {
        for (u64 x = renderer.cursor_position.x - 8; x < renderer.cursor_position.x; x++)
        {
            *(u32*)(pix_writer + x + (y * renderer.fb->pixels_per_scanline)) = renderer.clear_color;
        }
    }
    renderer.cursor_position.x -= 8;

    if (renderer.cursor_position.x < 0)
    {
        renderer.cursor_position.x = renderer.fb->width;
        renderer.cursor_position.y -= 16;
        if (renderer.cursor_position.y < 0)
        {
            renderer.cursor_position.y = 0;
        }
    }
}

static inline EfiMemoryDescriptor* get_descriptor(EFIMmap mmap, u64 index)
{
    EfiMemoryDescriptor* descriptor = (EfiMemoryDescriptor*)((u64)mmap.handle + (index * mmap.descriptor_size));
    return descriptor;
}

u64 get_memory_size(EFIMmap mmap)
{
    static u64 memory_size_bytes = 0;
    if (memory_size_bytes > 0)
    {
        return memory_size_bytes;
    }

    u64 mmap_entries = mmap.size / mmap.descriptor_size;

    for (u64 i = 0; i < mmap_entries; i++)
    {
        EfiMemoryDescriptor* descriptor = get_descriptor(mmap, i);
        memory_size_bytes += descriptor->page_count * 4096;
    }

    return memory_size_bytes;
}

void BitMap_init(usize bitmap_size, void* buffer_address)
{
    page_map.size = bitmap_size;
    page_map.buffer = (u8*)buffer_address;
    memset(page_map.buffer, 0, bitmap_size);
}

void free_page(void* address)
{
    u64 index = (u64)address / 4096;

    if (get_bit(page_map, index))
    {
        if (set_bit(page_map, index, false))
        {
            free_memory += 4096;
            used_memory -= 4096;
            if (last_page_map_index > index)
            {
                last_page_map_index = index;
            }
        }
    }
}

void free_pages(void* address, u64 page_count)
{
    for (u64 i = 0; i < page_count; i++)
    {
        void* page = (void*) ((u64)address + (i * 4096));
        free_page(page);
    }
}

void lock_page(void* address)
{
    u64 index = (u64)address / 4096;

    if (!get_bit(page_map, index))
    {
        if (set_bit(page_map, index, true))
        {
            free_memory -= 4096;
            used_memory += 4096;
        }
    }
}

void lock_pages(void* address, u64 page_count)
{
    for (u64 i = 0; i < page_count; i++)
    {
        void* page = (void*) ((u64)address + (i * 4096));
        lock_page(page);
    }
}

void reserve_page(void* address)
{
    u64 index = (u64)address / 4096;

    if (!get_bit(page_map, index))
    {
        if (set_bit(page_map, index, true))
        {
            free_memory -= 4096;
            reserved_memory += 4096;
        }
    }
}

void reserve_pages(void* address, u64 page_count)
{
    for (u64 i = 0; i < page_count; i++)
    {
        void* page = (void*) ((u64)address + (i * 4096));
        reserve_page(page);
    }
}

void unreserve_page(void* address)
{
    u64 index = (u64)address / 4096;

    if (get_bit(page_map, index))
    {
        if (set_bit(page_map, index, false))
        {
            free_memory += 4096;
            reserved_memory -= 4096;
            if (last_page_map_index > index)
            {
                last_page_map_index = index;
            }
        }
    }
}

void unreserve_pages(void* address, u64 page_count)
{
    for (u64 i = 0; i < page_count; i++)
    {
        void* page = (void*) ((u64)address + (i * 4096));
        unreserve_page(page);
    }
}

void* request_page(void)
{
    u64 count = page_map.size * 8;

    while (last_page_map_index < count)
    {
        if (!get_bit(page_map, last_page_map_index))
        {
            void* page = (void*)(last_page_map_index * 4096);
            lock_page(page);
            return page;
        }
        last_page_map_index++;
    }

    // @TODO: page frame swap to file
    return NULL;
}

void read_EFI_mmap(EFIMmap mmap)
{
    u64 mmap_entries = mmap.size / mmap.descriptor_size;

    void* largest_free_memory_segment = NULL;
    usize largest_free_memory_segment_size = 0;
    for (u64 i = 0; i < mmap_entries; i++)
    {
        EfiMemoryDescriptor* descriptor = get_descriptor(mmap, i);
        if (descriptor->type == EfiConventionalMemory)
        {
            u64 segment_size = descriptor->page_count * 4096;
            if (segment_size > largest_free_memory_segment_size)
            {
                largest_free_memory_segment = descriptor->physical_address;
                largest_free_memory_segment_size = segment_size;
            }
        }
    }

    u64 memory_size = get_memory_size(mmap);
    free_memory = memory_size;

    u64 bitmap_size = memory_size / 4096 / 8 + 1;

    BitMap_init(bitmap_size, largest_free_memory_segment);

    lock_pages(page_map.buffer, page_map.size / 4096 + 1);

    for (u32 i = 0; i < mmap_entries; i++)
    {
        EfiMemoryDescriptor* descriptor = get_descriptor(mmap, i);
        if (descriptor->type != EfiConventionalMemory)
        {
            reserve_pages(descriptor->physical_address, descriptor->page_count);
        }
    }
}

u64 get_free_RAM(void)
{
    return free_memory;
}
u64 get_used_RAM(void)
{
    return used_memory;
}
u64 get_reserved_RAM(void)
{
    return reserved_memory;
}

void print_memory_usage(void)
{
    print("Free RAM: ");
    print(unsigned_to_string(get_free_RAM() / 1024));
    print(" KB");
    new_line();
    print("Used RAM: ");
    print(unsigned_to_string(get_used_RAM() / 1024));
    print(" KB");
    new_line();
    print("Reserved RAM: ");
    print(unsigned_to_string(get_reserved_RAM() / 1024));
    print(" KB");
    new_line();
    print("Kernel start address: ");
    print(hex_to_string(_KernelStart));
    new_line();
    print("Kernel end address:   ");
    print(hex_to_string(_KernelEnd));
    new_line();
    print("Kernel size: ");
    print(unsigned_to_string(kernel_size / 1024));
    print(" KB");
    new_line();
}

void memmap(void* virtual_memory, void* physical_memory)
{
    PageMapIndexer pmi;
    {
        u64 virtual_address = (u64)virtual_memory;
        virtual_address >>= 12;
        pmi.P_i = virtual_address & 0x1ff;
        virtual_address >>= 9;
        pmi.PT_i = virtual_address & 0x1ff;
        virtual_address >>= 9;
        pmi.PD_i = virtual_address & 0x1ff;
        virtual_address >>= 9;
        pmi.PDP_i = virtual_address & 0x1ff;
    }

    PageDirectoryEntry PDE = PML4->entries[pmi.PDP_i];
    PageTable* PDP;

    if (!PDE_get_bit(PDE, PDEBit_Present))
    {
        PDP = (PageTable*)request_page();
        memset(PDP, 0, 0x1000);
        PDE_set_address(&PDE, (u64)PDP >> 12);
        PDE_set_bit(&PDE, PDEBit_Present, true);
        PDE_set_bit(&PDE, PDEBit_ReadWrite, true);
        PML4->entries[pmi.PDP_i] = PDE;
    }
    else
    {
        PDP = (PageTable*)(PDE_get_address(PDE) << 12);
    }

    PDE = PDP->entries[pmi.PD_i];
    PageTable* PD;

    if (!PDE_get_bit(PDE, PDEBit_Present))
    {
        PD = (PageTable*)request_page();
        memset(PD, 0, 0x1000);
        PDE_set_address(&PDE, (u64)PD >> 12);
        PDE_set_bit(&PDE, PDEBit_Present, true);
        PDE_set_bit(&PDE, PDEBit_ReadWrite, true);
        PDP->entries[pmi.PD_i] = PDE;
    }
    else
    {
        PD = (PageTable*)(PDE_get_address(PDE) << 12);
    }

    PDE = PD->entries[pmi.PT_i];
    PageTable* PT;

    if (!PDE_get_bit(PDE, PDEBit_Present))
    {
        PT = (PageTable*)request_page();
        memset(PT, 0, 0x1000);
        PDE_set_address(&PDE, (u64)PT >> 12);
        PDE_set_bit(&PDE, PDEBit_Present, true);
        PDE_set_bit(&PDE, PDEBit_ReadWrite, true);
        PD->entries[pmi.PT_i] = PDE;
    }
    else
    {
        PT = (PageTable*)(PDE_get_address(PDE) << 12);
    }

    PDE = PT->entries[pmi.P_i];
    PDE_set_address(&PDE, (u64)physical_memory >> 12);
    PDE_set_bit(&PDE, PDEBit_Present, true);
    PDE_set_bit(&PDE, PDEBit_ReadWrite, true);
    PT->entries[pmi.P_i] = PDE;
}

void IDTDescriptor_set_offset(IDTDescriptor* d, u64 offset)
{
    d->offset0 = (u16) (offset & 0x000000000000ffff);
    d->offset1 = (u16)((offset & 0x00000000ffff0000) >> 16);
    d->offset2 = (u32)((offset & 0xffffffff00000000) >> 32);
}

u64 IDTDescriptor_get_offset(IDTDescriptor d)
{
    u64 offset = (u64)d.offset0 | ((u64)d.offset1 << 16) | ((u64)d.offset2 << 32);
    return offset;
}

void IDT_gate_new(void* handler, u8 entry_offset, u8 type_attribute, u8 selector)
{
    IDTDescriptor* i = (IDTDescriptor*)(idtr.offset + entry_offset * sizeof(IDTDescriptor));
    IDTDescriptor_set_offset(i, (u64)handler);
    i->type_attribute = type_attribute;
    i->selector = selector;
}

void IDT_load(void)
{
    asm("lidt %0" : : "m" (idtr));
}

void interrupts_setup(void)
{
    idtr.limit = 0x0FFF;
    idtr.offset = (u64)request_page();

    IDT_gate_new(page_fault_handler, 0xE, IDT_TA_InterruptGate, 0x08);
    IDT_gate_new(double_fault_handler, 0x8, IDT_TA_InterruptGate, 0x08);
    IDT_gate_new(general_protection_fault_handler, 0xD, IDT_TA_InterruptGate, 0x08);
    IDT_gate_new(keyboard_handler, 0x21, IDT_TA_InterruptGate, 0x08);
    IDT_gate_new(mouse_handler, 0x2C, IDT_TA_InterruptGate, 0x08);

    IDT_load();
}

void memory_setup(BootInfo boot_info)
{
    kernel_size = (u64)&_KernelEnd - (u64)&_KernelStart;

    kernel_page_count = (u64) kernel_size / 4096 + 1;
    read_EFI_mmap(boot_info.mmap);
    lock_pages(&_KernelStart, kernel_page_count);


    PML4 = (PageTable*)request_page();
    memset(PML4, 0, 0x1000);

    u64 memsize = get_memory_size(boot_info.mmap);

    for (u64 i = 0; i < memsize; i += 0x1000)
    {
        memmap((void*)i, (void*)i);
    }

    u64 fb_base_address = (u64)renderer.fb->base_address;
    u64 fb_size = (u64)renderer.fb->size + 0x1000;

    lock_pages((void*)fb_base_address, fb_size / 0x1000 + 1);

    for (u64 i = fb_base_address; i < fb_base_address + fb_size; i += 0x1000)
    {
        memmap((void*)i, (void*)i);
    }

    asm volatile("mov %0, %%cr3" : : "r" (PML4));
}

void panic(const char* message)
{
    renderer.clear_color = Color_Red;
    fb_clear();
    renderer.cursor_position = (Point){0};
    renderer.color = Color_Black;
    println("Kernel panic");
    new_line();
    println(message);
}

void reset_terminal(void);
void ACPI_setup(BootInfo boot_info);
ACPI_SDT_Header* ACPI_find_table(ACPI_SDT_Header* xsdt_header, const char* signature);
void APIC_setup(void);

void MADT_explore(ACPI_MADT_Header* MADT_header)
{
    u64 LAPIC_address = MADT_header->LAPIC_address;
    bool LAPIC_address_override = false;
    bool dual_legacy_PICS_installed = (MADT_header->flags & 0x01) == 1;
    print("Dual legacy PICS installed: ");
    println(dual_legacy_PICS_installed? "true" : "false");
    u32 length = MADT_header->header.length;
    ACPI_MADT_EntryHeader* end_of_MADT = (ACPI_MADT_EntryHeader*) ((u64)MADT_header + length);

    u32 entry_count = 0;

    for (ACPI_MADT_EntryHeader* it = (ACPI_MADT_EntryHeader*)(MADT_header + 1);
            it != end_of_MADT;
            it = (ACPI_MADT_EntryHeader*) ((u64)it + it->record_length), entry_count++)
    {
        u8 entry_type = it->entry_type;
        switch (entry_type)
        {
            case MADT_EntryType_LAPIC:
            {
                ACPI_MADT_LAPIC_Entry* lapic = (ACPI_MADT_LAPIC_Entry*) it;
                u8 processor_id = lapic->ACPI_processor_ID;
                u8 APIC_id = lapic->APIC_ID;
                u32 flags = lapic->flags;

                println("MADT LAPIC entry:");
                print("* ACPI processor ID: ");
                println(unsigned_to_string(processor_id));
                print("* APIC ID: ");
                println(unsigned_to_string(APIC_id));
                print("* Flags: ");
                println(unsigned_to_string(flags));

                break;
            }
            case MADT_EntryType_IO_APIC:
            {
                ACPI_MADT_IO_APIC_Entry* ioapic = (ACPI_MADT_IO_APIC_Entry*) it;
                u8 id = ioapic->IO_APIC_ID;
                u32 address = ioapic->IO_APIC_address;
                u32 gsi = ioapic->global_system_interrupt_base;

                println("MADT IO APIC entry:");
                print("* APIC ID: ");
                println(unsigned_to_string(id));
                print("* Address: ");
                println_hex(address);
                print("* Global System Interrupt base: ");
                println_hex(gsi);

                break;
            }
            case MADT_EntryType_InterruptSourceOverride:
            {
                ACPI_MADT_InterruptSourceOverride_Entry* iso = (ACPI_MADT_InterruptSourceOverride_Entry*)it;
                u8 bus_source = iso->bus_source;
                u8 IRQ_source = iso->IRQ_source;
                u32 gsi = iso->global_system_interrupt;
                u16 flags = iso->flags;

                println("MADT Interrupt Source Override entry:");
                print("* Bus source: ");
                println(unsigned_to_string(bus_source));
                print("* IRQ source: ");
                println(unsigned_to_string(IRQ_source));
                print("* Global System Interrupt base: ");
                println_hex(gsi);
                print("* Flags: ");
                println_hex(flags);

                break;
            }
            case MADT_EntryType_NonMaskableInterrupts:
            {
                ACPI_MADT_NonMaskableInterrupts_Entry* nmi = (ACPI_MADT_NonMaskableInterrupts_Entry*)it;
                u8 id = nmi->ACPI_processor_ID;
                u16 flags = nmi->flags;
                u8 LINT = nmi->LINT;

                println("MADT Non-Maskable Interrupts entry:");
                print("* ACPI processor ID: ");
                println(unsigned_to_string(id));
                print("* Flags: ");
                println(unsigned_to_string(flags));
                print("* LINT: ");
                println(unsigned_to_string(LINT));
                break;
            }
            case MADT_EntryType_LAPIC_AddressOverride:
            {
                ACPI_MADT_LAPIC_AddressOverride_Entry* lapic_ao =  (ACPI_MADT_LAPIC_AddressOverride_Entry*)it;
                u64 address = lapic_ao->LAPIC_physical_address;

                println("MADT LAPIC Address Override entry:");
                print("LAPIC physical address: ");
                println_hex(address);
                LAPIC_address_override = true;
                LAPIC_address = address;

                break;
            }
            default:
                panic("Unknown MADT entry type");
                break;
        }
    }

    print("Local APIC address: ");
    print_hex(LAPIC_address);
    print(" (override: ");
    println(LAPIC_address_override? "true)" : "false)");
    LAPIC = (LAPICRegisters*) LAPIC_address;

    print("MADT entry count: ");
    println(unsigned_to_string(entry_count));
    new_line();

}

void HPET_explore(ACPI_HPET_Header* HPET_header)
{
    HPET = (HPETRegisters*) HPET_header->address;
}

void ACPI_setup(BootInfo boot_info)
{
    /*print("ACPI version: ");*/
    /*println(unsigned_to_string(boot_info.rsdp->descriptor1.revision));*/

    ACPI_SDT_Header* xsdt_header = (ACPI_SDT_Header*)boot_info.rsdp->XSDT_address;

    /*u8 sum = 0;*/
    /*for (u32 i = 0; i < xsdt_header->length; i++)*/
    /*{*/
        /*sum += ((char*)xsdt_header)[i];*/
    /*}*/
    /*if (sum == 0)*/
    /*{*/
        /*println("Valid XSDT checksum");*/
    /*}*/
    /*else*/
    /*{*/
        /*panic("Invalid XSDT checksum");*/
    /*}*/

    /*ACPI_print_tables(xsdt_header);*/

    /*ACPI_SDT_Header* MCFG_header = ACPI_find_table(xsdt_header, "MCFG");*/
    /*if (MCFG_header)*/
    /*{*/
        /*println("Found MCFG");*/
    /*}*/
    /*else*/
    /*{*/
        /*panic("MCFG not found");*/
    /*}*/

    /*PCI_enumerate((ACPI_MCFG_Header*)MCFG_header);*/

    ACPI_SDT_Header* MADT_header = ACPI_find_table(xsdt_header, "APIC");
    if (MADT_header)
    {
        println("Found MADT");
    }
    else
    {
        panic("MADT not found");
    }

    ACPI_SDT_Header* HPET_header = ACPI_find_table(xsdt_header, "HPET");
    if (HPET_header)
    {
        println("Found HPET");
    }
    else
    {
        println("HPET not found");
    }

    MADT_explore((ACPI_MADT_Header*) MADT_header);
    HPET_explore((ACPI_HPET_Header*) HPET_header);
}

void kernel_init(BootInfo boot_info)
{

    renderer = (const Renderer)
    {
        .fb = boot_info.framebuffer,
        .font = boot_info.font,
        .color = Color_White,
        .clear_color = Color_Black,
        .cursor_position = { .x = 0, .y = 0, },
    };

    memory_setup(boot_info);

    fb_clear();
    interrupts_setup();
    ACPI_setup(boot_info);
    load_gdt(&(GDTDescriptor) { .size = sizeof(GDT) - 1, .offset = (u64)&default_GDT, });
    APIC_setup();

    PS2_mouse_init();



    outb(PIC1_DATA, 0b11111001);
    outb(PIC2_DATA, 0b11101111);

    println("Hello UEFI x86_64 kernel!");
    print_memory_usage();
    //println(hex_to_string((u64)boot_info.rsdp));

    reset_terminal();
}

void clear_mouse_cursor(u8* mouse_cursor, Point position)
{
    if (mouse_never_drawn)
    {
        return;
    }

    s64 x_max = 16;
    s64 y_max = 16;
    s64 difference_x = renderer.fb->width - position.x;
    s64 difference_y = renderer.fb->height - position.y;

    if (difference_x < 16)
    {
        x_max = difference_x;
    }

    if (difference_y < 16)
    {
        y_max = difference_y;
    }

    for (s64 y = 0; y < y_max; y++)
    {
        for (s64 x = 0; x < x_max; x++)
        {
            s64 bit = y * 16 + x;
            s64 byte = bit / 8;
            if ((mouse_cursor[byte] & (0b10000000 >> (x % 8))) && (get_pixel(position.x + x, position.y + y) == mouse_cursor_buffer_post_render[x + y * 16]))
            {
                put_pixel(position.x + x, position.y + y, mouse_cursor_buffer[x + y * 16]);
            }
        }
    }
}

void draw_overlay_mouse_cursor(u8* mouse_cursor, Point position, Color color)
{
    s64 x_max = 16;
    s64 y_max = 16;
    s64 difference_x = renderer.fb->width - position.x;
    s64 difference_y = renderer.fb->height - position.y;

    if (difference_x < 16)
    {
        x_max = difference_x;
    }

    if (difference_y < 16)
    {
        y_max = difference_y;
    }

    for (s64 y = 0; y < y_max; y++)
    {
        for (s64 x = 0; x < x_max; x++)
        {
            s64 bit = y * 16 + x;
            s64 byte = bit / 8;
            if (mouse_cursor[byte] & (0b10000000 >> (x % 8)))
            {
                mouse_cursor_buffer[x + y * 16] = get_pixel(position.x + x, position.y + y);
                put_pixel(position.x + x, position.y + y, color);
                mouse_cursor_buffer_post_render[x + y * 16] = get_pixel(position.x + x, position.y + y);
            }
        }
    }

    mouse_never_drawn = false;
}

void PS2_mouse_process_packet(void)
{
    if (!mouse_packet_ready)
    {
        return;
    }

    mouse_packet_ready = false;

    bool x_neg = mouse_packet[0] & PS2XSign;
    bool y_neg = mouse_packet[0] & PS2YSign;
    bool x_overflow = mouse_packet[0] & PS2XOverflow;
    bool y_overflow = mouse_packet[0] & PS2YOverflow;

    if (!x_neg)
    {
        mouse_position.x += mouse_packet[1];
        if (x_overflow)
        {
            mouse_position.x += 255;
        }
    }
    else
    {
        mouse_packet[1] = 256 - mouse_packet[1];
        mouse_position.x -= mouse_packet[1];
        if (x_overflow)
        {
            mouse_position.x -= 255;
        }
    }

    if (!y_neg)
    {
        mouse_position.y -= mouse_packet[2];
        if (y_overflow)
        {
            mouse_position.y -= 255;
        }
    }
    else
    {
        mouse_packet[2] = 256 - mouse_packet[2];
        mouse_position.y += mouse_packet[2];
        if (y_overflow)
        {
            mouse_position.y += 255;
        }
    }

    if (mouse_position.x < 0)
    {
        mouse_position.x = 0;
    }

    if (mouse_position.x > renderer.fb->width - 1)
    {
        mouse_position.x = renderer.fb->width - 1;
    }

    if (mouse_position.y < 0)
    {
        mouse_position.y = 0;
    }

    if (mouse_position.y > renderer.fb->height - 1)
    {
        mouse_position.y = renderer.fb->height - 1;
    }

    clear_mouse_cursor(mouse_pointer, old_mouse_position);
    draw_overlay_mouse_cursor(mouse_pointer, mouse_position, Color_White);

    if (mouse_packet[0] & PS2LeftButton)
    {
#if 0
        print("[");
        print(unsigned_to_string(renderer.cursor_position.x));
        print(",");
        print(unsigned_to_string(renderer.cursor_position.y));
        println("], ");
#else
        allow_keyboard_input = !allow_keyboard_input;
#endif
    }

    if (mouse_packet[0] & PS2MiddleButton)
    {

    }

    if (mouse_packet[0] & PS2RightButton)
    {
#if 0
        Color color = renderer.color;
        renderer.color = Color_Blue;
        put_char_in_point('a', mouse_position.x, mouse_position.y);
        renderer.color = color;
#else
        scroll(&renderer);
#endif
    }

    mouse_packet_ready = false;
    old_mouse_position = mouse_position;
}

void reset_terminal(void)
{
    // If the command is empty, we do not need to record it
    if (cmd_buffer[current_command].char_count)
    {
        if (current_command <= array_length(cmd_buffer))
        {
            current_command++;
        }
        else
        {
            memcpy(cmd_buffer, cmd_buffer + 1, sizeof(cmd_buffer) - sizeof(TerminalCommandBuffer));
        }

        cmd_buffer[current_command].char_count = 0;
    }

    new_line();
    allow_keyboard_input = true;
    print("> ");
}

void kb_input_process(void)
{

    KeyboardBuffer kb_buffer;
    u16 kb_event_count;
    // This cleans kb interrupt buffer
    bool overflow = kb_get_buffer(&kb_buffer, &kb_event_count);
    if (overflow)
    {
        println("KEYBOARD BUFFER OVERFLOW");
        while(1);
    }

    if (!allow_keyboard_input)
    {
        return;
    }

    for (u16 i = 0; i < kb_event_count; i++)
    {
        u8 scancode = kb_buffer.messages[i];
        switch (scancode)
        {
            case Key_LeftShift:
                left_shift_pressed = true;
                break;
            case Key_LeftShift + 0x80:
                left_shift_pressed = false;
                break;
            case Key_RightShift:
                right_shift_pressed = true;
                break;
            case Key_RightShift + 0x80:
                right_shift_pressed = false;
                break;
            case Key_Enter:
                new_line();
                allow_keyboard_input = false;
                return;
            case Key_Backspace:
                if (cmd_buffer[current_command].char_count)
                {
                    clear_char();
                    cmd_buffer[current_command].char_count--;
                }
                break;
            default:
            {
                char ch = translate_scancode(scancode, left_shift_pressed || right_shift_pressed);
                if (ch)
                {
                    putc(ch);

                    bool overflow = cmd_buffer[current_command].char_count + 1 > array_length(cmd_buffer[current_command].characters);
                    if (overflow)
                    {
                        println("Command buffer overflow");
                        reset_terminal();
                        return;
                    }

                    cmd_buffer[current_command].characters[cmd_buffer[current_command].char_count++] = ch;
                }
            }
        }
    }
}

char* strcpy(char* dst, const char* src)
{
    if (src && dst)
    {
        char* src_it = (char*)src;
        char* dst_it = dst;
        while (*src_it)
        {
            *dst_it++ = *src_it++;
        }
        *dst_it = *src_it;
    }

    return dst;
}

s32 strcmp(const char* lhs, const char* rhs)
{
    char* lit = (char*)lhs;
    char* rit = (char*)rhs;

    while (*lit == *rit)
    {
        if (*lit == 0)
        {
            return 0;
        }
        lit++;
        rit++;
    }

    return *lit - *rit;
}

s32 strncmp(const char* lhs, const char* rhs, usize count)
{
    char* lit = (char*)lhs;
    char* rit = (char*)rhs;
    bool eq;

    for (usize i = 0; i < count && (eq = *lit == *rit); i++, lit++, rit++)
    {
        if (*lit == 0)
        {
            return 0;
        }
    }

    return (*lit - *rit) * (!eq);
}

usize strlen(const char* s)
{
    char* it = (char*)s;

    while (*it)
    {
        it++;
    }

    return it - s;
}

bool string_eq(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

Command parse_command(char* raw_buffer)
{
    Command cmd = {0};
    char* it = raw_buffer;

    while (*it && *it != ' ')
    {
        cmd.name[it - raw_buffer] = *it;
        it++;
    }
    cmd.name[it - raw_buffer] = 0;

    for (u32 i = 0; *it; i++)
    {
        // Skip spaces
        while (*it && *it == ' ')
        {
            it++;
        }

        u32 c;
        for (c = 0; *it && *it != ' '; c++, it++)
        {
            cmd.args[i][c] = *it;
        }
        cmd.args[i][c] = 0;
    }

    return cmd;
}

// Reverse copy the string (including the null-terminated character
void reverse(char* dst, const char* src, usize bytes)
{
    for (u32 i = 0; i < bytes; i++)
    {
        dst[i] = src[bytes - i - 1];
    }
    dst[bytes] = 0;
}

u64 string_to_unsigned(const char* str)
{
    FormatLookupTableIndex table_index = DECIMAL;
    usize len = strlen(str);
    char* it = (char*) str;

    if (it[0] == '0' && it[1] == 'x')
    {
        table_index = HEXADECIMAL;
        len -= 2;
        it += 2;
    }
    else if (it[0] == '0' && it[1] == 'b')
    {
        table_index = BINARY;
        len -= 2;
        it += 2;
    }

    const char* lookup_table = format_lookup_table[table_index];
    u64 values_per_digit = format_values_per_digit[table_index];

    char copy[65];
    reverse(copy, it, len);

    u64 result = 0;
    u64 byte_multiplier = 1;

    for (u32 i = 0; i < len; i++, byte_multiplier *= values_per_digit)
    {
        u64 lookup_value = lookup_table[copy[i]];
        u64 mul = lookup_value * byte_multiplier;
        result += mul;
    }

    return result;
}

void cmd_ls(Command* cmd)
{
    println("Filesystem is not implemented yet");
}

void cmd_memdump(Command* cmd)
{
    u64 mem = string_to_unsigned(cmd->args[0]);
    u64 bytes = string_to_unsigned(cmd->args[1]);

    u8* it = (u8*)mem;

    u8 divide_every = 4;

    for (u64 i = 0; i < bytes; i++)
    {
        u8* ptr = it + i;
        print(hex_to_string((u64)ptr));
        print(": ");
        print(hex_to_string(*ptr));
        bool end_of_line = ((i > 0 && i % divide_every == 0) || i == bytes - 1);
        if (end_of_line)
        {
            new_line();
        }
        else
        {
            print(" | ");
        }
    }
}

void process_command(void)
{
    Command cmd = parse_command(cmd_buffer[current_command].characters);
    for (u32 i = 0; i < array_length(kernel_commands); i++)
    {
        if (string_eq(kernel_commands[i].name, cmd.name))
        {
            kernel_commands[i].dispatcher(&cmd);
            return;
        }
    }

    println("Unknown command");
}

void ACPI_print_tables(ACPI_SDT_Header* xsdt_header)
{
    u32 table_count = (xsdt_header->length - sizeof(ACPI_SDT_Header)) / 8;
    // Point to the end of the header (the beginning of the tables
    u64* pointer_table = (u64*)(xsdt_header + 1);

    print("ACPI tables: ");
    println(unsigned_to_string(table_count));
    for (u32 i = 0; i < table_count; i++)
    {
        print("Table ");
        print(unsigned_to_string(i));
        print(": ");
        ACPI_SDT_Header* table_header = (ACPI_SDT_Header*) pointer_table[i];
        for (u32 c = 0; c < sizeof(table_header->signature); c++)
        {
            putc(table_header->signature[c]);
        }
        print(" @");
        print_hex((u64)table_header);
        new_line();
    }
}

ACPI_SDT_Header* ACPI_find_table(ACPI_SDT_Header* xsdt_header, const char* table_signature)
{
    u32 table_count = (xsdt_header->length - sizeof(ACPI_SDT_Header)) / 8;
    // Point to the end of the header (the beginning of the tables
    u64* pointer_table = (u64*)(xsdt_header + 1);

    for (u32 i = 0; i < table_count; i++)
    {
        ACPI_SDT_Header* table_header = (ACPI_SDT_Header*) pointer_table[i];
        if (memequal(table_header->signature, table_signature, sizeof(table_header->signature)))
        {
            return table_header;
        }
    }

    return NULL;
}

void PCI_enumerate_function(u64 device_address, u64 function)
{
    u64 offset = function << 12;
    u64 function_address = device_address + offset;

    memmap((void*)function_address, (void*)function_address);

    PCI_DeviceHeader* pci_device_header = (PCI_DeviceHeader*)function_address;
    if (pci_device_header->device_ID == 0 || pci_device_header->device_ID == 0xffff)
    {
        return;
    }

    print("Device ID: ");
    print(hex_to_string(pci_device_header->device_ID));
    print(". Vendor ID: ");
    println(hex_to_string(pci_device_header->vendor_ID));
}

void PCI_enumerate_device(u64 bus_address, u64 device)
{
    u64 offset = device << 15;
    u64 device_address = bus_address + offset;

    memmap((void*)device_address, (void*)device_address);

    PCI_DeviceHeader* pci_device_header = (PCI_DeviceHeader*)device_address;
    if (pci_device_header->device_ID == 0 || pci_device_header->device_ID == 0xffff)
    {
        return;
    }

    for (u64 function = 0; function < 8; function++)
    {
        PCI_enumerate_function(device_address, function);
    }
}

void PCI_enumerate_bus(u64 base_address, u64 bus)
{
    u64 offset = bus << 20;
    u64 bus_address = base_address + offset;

    memmap((void*)bus_address, (void*)bus_address);

    PCI_DeviceHeader* pci_device_header = (PCI_DeviceHeader*)bus_address;
    if (pci_device_header->device_ID == 0 || pci_device_header->device_ID == 0xffff)
    {
        return;
    }

#define PCI_DEVICE_COUNT_PER_BUS 32
    for (u64 device = 0; device < PCI_DEVICE_COUNT_PER_BUS; device++)
    {
        PCI_enumerate_device(bus_address, device);
    }
}

void PCI_enumerate(ACPI_MCFG_Header* mcfg_header)
{
    new_line();

    u32 mcfg_entries = (mcfg_header->header.length - sizeof(ACPI_MCFG_Header)) / sizeof(ACPI_DeviceConfig);
    print("MCFG entries: ");
    println(unsigned_to_string(mcfg_entries));
    println("Listing PCI devices:");

    ACPI_DeviceConfig* device_config_array = (ACPI_DeviceConfig*)(mcfg_header + 1);
    memmap(device_config_array, device_config_array);

    for (u32 i = 0; i < mcfg_entries; i++)
    {
        ACPI_DeviceConfig* device_cfg = (ACPI_DeviceConfig*)&device_config_array[i];
        memmap(device_cfg, device_cfg);

        for (u64 bus = device_cfg->start_bus; bus < device_cfg->end_bus; bus++)
        {
            PCI_enumerate_bus(device_cfg->base_address, bus);
        }
    }

    new_line();
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

#define assert(x) _assert(x, #x)

void _assert(bool condition, const char* condition_name)
{
    if (!condition)
    {
        panic(condition_name);
    }
}

void LAPIC_timer_setup(void)
{
    LAPIC->DCR_timer.reg = 0x03;
    PIT_prepare_sleep(10000);
    LAPIC->ICR_timer.reg = 0xFFFFFFFF;
    PIT_sleep();
    LAPIC->LVT_Timer.reg = APIC_LVT_INT_MASKED;
    u32 tick_count = 0xFFFFFFFF - LAPIC->CCR_timer.reg;

    LAPIC->LVT_Timer.reg = 32 | APIC_LVT_TIMER_MODE_PERIODIC;
    LAPIC->DCR_timer.reg = 0x03;
    u32 value = tick_count / 10;
    u64 address = (u64)&LAPIC->ICR_timer.reg;
    while(1);
    LAPIC->ICR_timer.reg = value;
    println("LAPIC timer initialized!");
    print("Tick count: ");
    println(unsigned_to_string(tick_count));
}

void LAPIC_setup(void)
{
    memmap((void*)LAPIC, (void*)LAPIC);
    LAPIC->DFR.reg = 0xFFFFFFFF;
    LAPIC->LDR.reg = (LAPIC->LDR.reg & 0x00FFFFFF) | 1;
    LAPIC->LVT_Timer.reg = APIC_LVT_INT_MASKED;
    LAPIC->LVT_PMC.reg = APIC_LVT_DELIVERY_MODE_NMI;
    LAPIC->LVT_LINT0.reg = APIC_LVT_INT_MASKED;
    LAPIC->LVT_LINT1.reg = APIC_LVT_INT_MASKED;
    LAPIC->TPR.reg = 0;

    LAPIC->SIV.reg = 0xFF | APIC_SPURIOUS_IVT_SOFTWARE_ENABLE;
    println("LAPIC successfully initialized!");
}

void HPET_setup(void)
{
    memmap((void*)HPET, (void*)HPET);
    print("HPET address: ");
    println_hex((u64)HPET);
    u32 period = HPET->cap_and_ID.counter_clk_period;
    HPET_frequency /= period;

    print("HPET period:    ");
    print(unsigned_to_string(period));
    println(" femtoseconds");
    print("HPET frequency: ");
    print(unsigned_to_string(HPET_frequency));
    println(" femtoseconds");

    HPET->configuration.enable_cnf = 1;
    print("HPET is enabled now: ");

    print("HPET enabled: ");
    println(HPET->configuration.enable_cnf ? "true" : "false");
    print("HPET legacy mode: ");
    println(HPET->configuration.leg_rt_cnf ? "true" : "false");
}


void APIC_setup(void)
{
    APIC_disable_PIC();

    LAPIC_setup();
    HPET_setup();
}

void _start(BootInfo boot_info)
{
    kernel_init(boot_info);

    while (true)
    {
        PS2_mouse_process_packet();
        kb_input_process();
        // This means we should process a command
        if (!allow_keyboard_input)
        {
            process_command();
            reset_terminal();
        }

        hlt();
    }
}
