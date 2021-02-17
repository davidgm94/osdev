#pragma once
#include "types.h"

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

ACPI_SDT_Header* ACPI_find_table(ACPI_SDT_Header* xsdt_header, const char* signature);
void ACPI_setup(ACPI_RSDPDescriptor2* rsdp);