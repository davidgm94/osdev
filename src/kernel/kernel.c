#include "config.h"
#include "types.h"
#include "asm.h"
#include "renderer_internal.h"
#include "libk.h"
#include "acpi.h"
#include "interrupts.h"
#include "keyboard.h"
#include "mouse.h"

bool allow_keyboard_input = true;

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





static PageTable* PML4; 
static BitMap page_map;
static u64 free_memory;
static u64 reserved_memory;
static u64 used_memory;
static u64 last_page_map_index = 0;


static TerminalCommandBuffer cmd_buffer[8];
static u8 current_command = 0;


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
    println("Free RAM: %64u KB", get_free_RAM() / 1024);
    println("Used RAM: %64u KB", get_used_RAM() / 1024);
    println("Reserved RAM: %64u KB", get_reserved_RAM() / 1024);
    println("Kernel start address: %64h", _KernelStart);
    println("Kernel end address:   %64h", _KernelEnd);
    println("Kernel size: %64u KB", kernel_size / 1024);
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

void reset_terminal(void);

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

    GDT_setup();
    interrupts_setup();

#if APIC
    ACPI_setup(boot_info.rsdp);
    APIC_setup();
#endif

    //PS2_mouse_init();

    println("Hello UEFI x86_64 kernel!");
    print_memory_usage();

    reset_terminal();
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



void cmd_ls(Command* cmd)
{
    println("Filesystem is not implemented yet");
}

void cmd_memdump(Command* cmd)
{
    u64 mem = string_to_unsigned(cmd->args[0]);
    u64 bytes = string_to_unsigned(cmd->args[1]);

    if (mem == 0 || bytes == 0)
    {
        println("Wrong usage");
    }

    u8* it = (u8*)mem;

    u8 divide_every = 4;

    for (u64 i = 0; i < bytes; i++)
    {
        u8* ptr = it + i;

        print("%64h: %8h", (u64)ptr, *ptr);

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


void kb_backspace_action(void)
{
    if (cmd_buffer[current_command].char_count)
    {
        clear_char();
        cmd_buffer[current_command].char_count--;
    }
}

void kb_print_ch(u8 scancode)
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

void KernelMain(BootInfo* boot_info)
{
    kernel_init(*boot_info);

    while (true)
    {
        //PS2_mouse_process_packet();
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
