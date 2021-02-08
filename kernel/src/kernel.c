#include "types.h"
#include "io.h"
#include "interrupts.h"

typedef struct Framebuffer
{
    void* base_address;
    usize size;
    u32 width, height;
    u32 pixels_per_scanline;
} Framebuffer;

// @TODO: first bit can cause problems, check out
typedef enum Color
{
    Color_Black = 0xff000000,
    Color_Red = 0x00ff0000,
    Color_Green = 0x0000ff00,
    Color_Blue = 0x000000ff,
    Color_White = Color_Blue | Color_Green | Color_Red,
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

typedef struct BooInfo
{
    Framebuffer* framebuffer;
    PSF1Font* font;
    EFIMmap mmap;
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

extern u64 _KernelStart;
extern u64 _KernelEnd;
static u64 kernel_size;
static u64 kernel_page_count;


ALIGN(0x1000) static GDT default_GDT = 
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

static BitMap page_map;
static u64 free_memory;
static u64 reserved_memory;
static u64 used_memory;
static u64 last_page_map_index = 0;

extern void load_gdt(GDTDescriptor* gdt_descriptor);

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

void new_line_ex(Renderer* renderer)
{
    renderer->cursor_position = (Point) { .x = 0, .y = renderer->cursor_position.y + 16 };
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

void render_char(Renderer* renderer, char c)
{
    u32* pix_writer = (u32*)renderer->fb->base_address;
    char* font_reader = renderer->font->glyph_buffer + (c * renderer->font->header->char_size);

    for (u64 y = renderer->cursor_position.y; y < renderer->cursor_position.y + 16; y++, font_reader++)
    {
        for (u64 x = renderer->cursor_position.x; x < renderer->cursor_position.x + 8; x++)
        {
            if ((*font_reader & (0b10000000 >> (x - renderer->cursor_position.x))) > 0)
            {
                *(u32*)(pix_writer + x + (y * renderer->fb->pixels_per_scanline)) = renderer->color;
            }
        }
    }
}

void print_ex(Renderer* renderer, char* str)
{
    if (str)
    {
        while (*str)
        {
            render_char(renderer, *str);
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
    render_char(&renderer, c);
    handle_newline_while_printing(&renderer);
}

void print(char* str)
{
    print_ex(&renderer, str);
}


void println(char* str)
{
    print(str);
    new_line();
}

char* unsigned_to_string(u64 value)
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

char* signed_to_string(s64 value)
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

char* float_to_string(f64 value, u8 decimal_digits)
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

char* hex_to_string(u64 value)
{
    u8 digits = sizeof(u64) * 2 - 1;
    
    for (u8 i = 0; i < digits; i++)
    {
        u8* ptr = ((u8*)&value + i);

        u8 tmp = ((*ptr & 0xF0) >> 4);
        hex_to_string_output[digits - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');

        tmp = (*ptr) & 0x0F;
        hex_to_string_output[digits - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }

    hex_to_string_output[digits + 1] = 0;

    return hex_to_string_output;
}

void fb_clear(void)
{
    Framebuffer* fb = renderer.fb;
    u64 fb_base = (u64)fb->base_address;
    u64 bytes_per_scanline = fb->pixels_per_scanline * 4;
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

    // @TODO: shouldn't it be page_map.buffer instead?
    lock_pages(&page_map, page_map.size / 4096 + 1);

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
    print("Kernel start address: 0x");
    print(hex_to_string(_KernelStart));
    new_line();
    print("Kernel end address:   0x");
    print(hex_to_string(_KernelEnd));
    new_line();
    print("Kernel size: ");
    print(unsigned_to_string(kernel_size / 1024));
    print(" KB");
    new_line();
}

void memmap(PageTable* PML4, void* virtual_memory, void* physical_memory)
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

void interrupts_setup(void)
{
    idtr.limit = 0x0FFF;
    idtr.offset = (u64)request_page();

    IDTDescriptor* int_page_fault = (IDTDescriptor*)(idtr.offset + 0xE * sizeof(IDTDescriptor));
    IDTDescriptor_set_offset(int_page_fault, (u64)page_fault_handler);
    int_page_fault->type_attribute = IDT_TA_InterruptGate;
    int_page_fault->selector = 0x08;

    IDTDescriptor* int_double_page_fault = (IDTDescriptor*)(idtr.offset + 0x8 * sizeof(IDTDescriptor));
    IDTDescriptor_set_offset(int_double_page_fault, (u64)double_fault_handler);
    int_double_page_fault->type_attribute = IDT_TA_InterruptGate;
    int_double_page_fault->selector = 0x08;

    IDTDescriptor* int_general_protection_fault = (IDTDescriptor*)(idtr.offset + 0xD * sizeof(IDTDescriptor));
    IDTDescriptor_set_offset(int_general_protection_fault, (u64)general_protection_fault_handler);
    int_general_protection_fault->type_attribute = IDT_TA_InterruptGate;
    int_general_protection_fault->selector = 0x08;

    IDTDescriptor* int_keyboard_handler = (IDTDescriptor*)(idtr.offset + 0x21 * sizeof(IDTDescriptor));
    IDTDescriptor_set_offset(int_keyboard_handler, (u64)keyboard_handler);
    int_keyboard_handler->type_attribute = IDT_TA_InterruptGate;
    int_keyboard_handler->selector = 0x08;

    asm("lidt %0" : : "m" (idtr));

    PIC_remap();

    outb(PIC1_DATA, 0b11111101);
    outb(PIC2_DATA, 0b11111111);

    asm("sti");
}

void memory_setup(BootInfo boot_info)
{
    kernel_size = (u64)&_KernelEnd - (u64)&_KernelStart;

    kernel_page_count = (u64) kernel_size / 4096 + 1;
    read_EFI_mmap(boot_info.mmap);
    lock_pages(&_KernelStart, kernel_page_count);


    PageTable* PML4 = (PageTable*)request_page();
    memset(PML4, 0, 0x1000);

    u64 memsize = get_memory_size(boot_info.mmap);

    for (u64 i = 0; i < memsize; i += 0x1000)
    {
        memmap(PML4, (void*)i, (void*)i);
    }

    u64 fb_base_address = (u64)renderer.fb->base_address;
    u64 fb_size = (u64)renderer.fb->size + 0x1000;

    lock_pages((void*)fb_base_address, fb_size / 0x1000 + 1);

    for (u64 i = fb_base_address; i < fb_base_address + fb_size; i += 0x1000)
    {
        memmap(PML4, (void*)i, (void*)i);
    }

    asm volatile("mov %0, %%cr3" : : "r" (PML4));
}

void panic(char* message)
{
    renderer.clear_color = Color_Red;
    fb_clear();
    renderer.cursor_position = (Point){0};
    renderer.color = Color_Black;
    println("Kernel panic");
    new_line();
    println(message);
}

void kernel_init(BootInfo boot_info)
{
    load_gdt(&(GDTDescriptor) { .size = sizeof(GDT) - 1, .offset = (u64)&default_GDT, });

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

    println("Hello UEFI x86_64 kernel!");
    print_memory_usage();
}

void _start(BootInfo boot_info)
{
    kernel_init(boot_info);

    for(;;);
}
