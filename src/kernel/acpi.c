#include "acpi.h"
#include "config.h"
#include "libk.h"
#include "panic.h"

extern void memmap(void*, void*);
extern u64 LAPIC_address;
extern u64 HPET_address;

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

void ACPI_print_tables(ACPI_SDT_Header* xsdt_header)
{
    u32 table_count = (xsdt_header->length - sizeof(ACPI_SDT_Header)) / 8;
    // Point to the end of the header (the beginning of the tables
    u64* pointer_table = (u64*)(xsdt_header + 1);

    println("ACPI tables: %32u", table_count);
    for (u32 i = 0; i < table_count; i++)
    {
        ACPI_SDT_Header* table_header = (ACPI_SDT_Header*) pointer_table[i];
        char* signature = table_header->signature;
        println("Table %32u: %c%c%c%c @%64h", i,
                signature[0], signature[1], signature[2], signature[3],
                (u64)table_header);
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

    println("Device ID: %16u. Vendor ID: %16u.", pci_device_header->device_ID, pci_device_header->vendor_ID);
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
    println("MCFG entries: %32u", mcfg_entries);
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

void MADT_explore(ACPI_MADT_Header* MADT_header)
{
    LAPIC_address = MADT_header->LAPIC_address;
    bool LAPIC_address_override = false;
    bool dual_legacy_PICS_installed = (MADT_header->flags & 0x01) == 1;
    println("Dual legacy PICS installed: %b", dual_legacy_PICS_installed);
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
                println("* ACPI processor ID: %8u", processor_id);
                println("* APIC ID: %8u", APIC_id);
                println("* Flags: %32h", flags);

                break;
            }
            case MADT_EntryType_IO_APIC:
            {
                ACPI_MADT_IO_APIC_Entry* ioapic = (ACPI_MADT_IO_APIC_Entry*) it;
                u8 id = ioapic->IO_APIC_ID;
                u32 address = ioapic->IO_APIC_address;
                u32 gsi = ioapic->global_system_interrupt_base;

                println("MADT IO APIC entry:");
                println("* APIC ID: %8u", id);
                println("* Address: %32h", address);
                println("* Global System Interrupt base: %32h", gsi);

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
                println("* Bus source: %8u", bus_source);
                println("* IRQ source: %8u", IRQ_source);
                println("* Global System Interrupt base: %32h", gsi);
                println("* Flags: %16h", flags);

                break;
            }
            case MADT_EntryType_NonMaskableInterrupts:
            {
                ACPI_MADT_NonMaskableInterrupts_Entry* nmi = (ACPI_MADT_NonMaskableInterrupts_Entry*)it;
                u8 id = nmi->ACPI_processor_ID;
                u16 flags = nmi->flags;
                u8 LINT = nmi->LINT;

                println("MADT Non-Maskable Interrupts entry:");
                println("* ACPI processor ID: %8u", id);
                println("* Flags: %16h", flags);
                println("* LINT: %8u", LINT);
                break;
            }
            case MADT_EntryType_LAPIC_AddressOverride:
            {
                ACPI_MADT_LAPIC_AddressOverride_Entry* lapic_ao =  (ACPI_MADT_LAPIC_AddressOverride_Entry*)it;
                u64 address = lapic_ao->LAPIC_physical_address;

                println("MADT LAPIC Address Override entry:");
                println("LAPIC physical address: %64h", address);
                LAPIC_address_override = true;
                LAPIC_address = address;

                break;
            }
            default:
                panic("Unknown MADT entry type");
                break;
        }
    }

    println("Local APIC address: %64h (override %b)", LAPIC_address, LAPIC_address_override);
    println("MADT entry count: %32u\n", entry_count);
}

void HPET_explore(ACPI_HPET_Header* HPET_header)
{
    HPET_address = HPET_header->address;
}

void ACPI_setup(ACPI_RSDPDescriptor2* rsdp)
{
    /*print("ACPI version: ");*/
    /*println(unsigned_to_string(boot_info.rsdp->descriptor1.revision));*/

    ACPI_SDT_Header* xsdt_header = (ACPI_SDT_Header*)rsdp->XSDT_address;

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
    /*if (MADT_header)*/
    /*{*/
        /*println("Found MADT");*/
    /*}*/
    /*else*/
    /*{*/
        /*panic("MADT not found");*/
    /*}*/

    ACPI_SDT_Header* HPET_header = ACPI_find_table(xsdt_header, "HPET");
    /*if (HPET_header)*/
    /*{*/
        /*println("Found HPET");*/
    /*}*/
    /*else*/
    /*{*/
        /*println("HPET not found");*/
    /*}*/

    MADT_explore((ACPI_MADT_Header*) MADT_header);
    HPET_explore((ACPI_HPET_Header*) HPET_header);
}