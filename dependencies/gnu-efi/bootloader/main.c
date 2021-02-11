#include <efi.h>
#include <efilib.h>
#include <elf.h>

typedef unsigned long long size_t;

typedef struct Framebuffer
{
    void* base_address;
    size_t size;
    unsigned int width, height;
    unsigned int pixels_per_scanline;
} Framebuffer;

unsigned char PSF1_magic[2] = { 0x36, 0x04 };
typedef struct PSF1Header
{
    unsigned char magic[2];
    unsigned char mode;
    unsigned char char_size;
} PSF1Header;

typedef struct PSF1Font
{
    PSF1Header* header;
    void* glyph_buffer;
} PSF1Font;

Framebuffer framebuffer;

EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable);
int memcmp(const void* mem1, const void* mem2, size_t bytes);
Framebuffer* InitializeGOP(void);
PSF1Font* LoadPSF1Font(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable);
EFI_STATUS efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable){
	EFI_FILE* LoadedFile;

	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	SystemTable->BootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);
    Print(L"Image loaded\n");

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
	SystemTable->BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);
    Print(L"Protocol handled\n");

	if (Directory == NULL)
    {
		FileSystem->OpenVolume(FileSystem, &Directory);
	}
    Print(L"Directory open\n");

	EFI_STATUS s = Directory->Open(Directory, &LoadedFile, Path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
    Print(L"File open called\n");
	if (s != EFI_SUCCESS)
    {
		return NULL;
	}
	return LoadedFile;

}

int memcmp(const void* mem1, const void* mem2, size_t bytes)
{
    const unsigned char* a = mem1, *b = mem2;
    for (size_t i = 0; i < bytes; i++)
    {
        if (a[i] < b[i]) return -1;
        else if (a[i] > b[i]) return 1;
    }
    return 0;
}

Framebuffer* InitializeGOP(void)
{
    EFI_GUID gopGUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGUID, NULL, (void**)&gop);
    if (EFI_ERROR(status))
    {
        Print(L"Unable to locate GOP\r\n");
        return NULL;
    }
    else
    {
        Print(L"GOP located\r\n");
    }

    framebuffer.base_address = (void*)gop->Mode->FrameBufferBase;
    framebuffer.size = gop->Mode->FrameBufferSize;
    framebuffer.width = gop->Mode->Info->HorizontalResolution;
    framebuffer.height = gop->Mode->Info->VerticalResolution;
    framebuffer.pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;

    return &framebuffer;
}

PSF1Font* LoadPSF1Font(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    EFI_FILE* font = LoadFile(Directory, Path, ImageHandle, SystemTable);
    if (!font)
    {
        return NULL;
    }

    PSF1Header* header;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF1Header), (void**)&header);
    UINTN size = sizeof(PSF1Header);
    font->Read(font, &size, header);
    if (header->magic[0] != PSF1_magic[0] || header->magic[1] != PSF1_magic[1])
    {
        return NULL;
    }

    UINTN glyph_buffer_size = header->char_size * 256;
    if (header->mode == 1)
    {
        glyph_buffer_size = header->char_size * 512;
    }

    void* glyph_buffer;
    font->SetPosition(font, sizeof(PSF1Header));
    SystemTable->BootServices->AllocatePool(EfiLoaderData, glyph_buffer_size, (void**)&glyph_buffer);
    font->Read(font, &glyph_buffer_size, glyph_buffer);

    PSF1Font* result;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF1Font), (void**)&result);
    result->header = header;
    result->glyph_buffer = glyph_buffer;

    return result;
}

UINTN strequal(CHAR8* a, CHAR8* b, UINTN length)
{
    for (UINTN i = 0; i < length; i++)
    {
        if (*a != *b)
        {
            return 0;
        }
    }

    return 1;
}

#define CHECK_KERNEL_FORMAT(cause, message) if (cause) { Print(L"Bad kernel format. Cause: "); Print(message); Print(L"\r\n"); return EFI_LOAD_ERROR; }
EFI_STATUS efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
	InitializeLib(ImageHandle, SystemTable);

	EFI_FILE* Kernel = LoadFile(NULL, L"kernel.elf", ImageHandle, SystemTable);
	if (Kernel == NULL)
    {
		Print(L"Could not load kernel \n\r");
	}
	else
    {
		Print(L"Kernel Loaded Successfully \n\r");
	}

    Elf64_Ehdr header;
    {
        UINTN FileInfoSize;
        EFI_FILE_INFO* FileInfo;
        Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, NULL);
        SystemTable->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);
        Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);

        UINTN size = sizeof(header);
        Kernel->Read(Kernel, &size, &header);
    }

    unsigned elf_mag_bit = (memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0);
    unsigned elf_class_64 = header.e_ident[EI_CLASS] != ELFCLASS64;
    unsigned elf_data_2_lsb = header.e_ident[EI_DATA] != ELFDATA2LSB;
    unsigned elf_executable = header.e_type != ET_EXEC;
    unsigned elf_machine_x86_64 = header.e_machine != EM_X86_64;
    unsigned elf_version = header.e_version != EV_CURRENT;

    CHECK_KERNEL_FORMAT(elf_mag_bit, L"elf_mag_bit");
    CHECK_KERNEL_FORMAT(elf_class_64, L"elf_class_64");
    CHECK_KERNEL_FORMAT(elf_data_2_lsb, L"elf_data_2_lsb");
    CHECK_KERNEL_FORMAT(elf_executable, L"elf_executable");
    CHECK_KERNEL_FORMAT(elf_machine_x86_64, L"elf_machine_x86_64");
    CHECK_KERNEL_FORMAT(elf_version, L"elf_version");

    Print(L"Kernel header successfully verified\r\n");

    Elf64_Phdr* phdrs;
    {
        Kernel->SetPosition(Kernel, header.e_phoff);
        UINTN size = header.e_phnum * header.e_phentsize;
        SystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&phdrs);
        Kernel->Read(Kernel, &size, phdrs);
    }

    for (Elf64_Phdr* phdr = phdrs; (char*)phdr < (char*)phdrs + header.e_phnum * header.e_phentsize; phdr = (Elf64_Phdr*)((char*)phdr + header.e_phentsize))
    {
        switch (phdr->p_type)
        {
            case PT_LOAD:
            {
                int pages = (phdr->p_memsz + 0x1000 - 1) / 0x1000;
                Elf64_Addr segment = phdr->p_paddr;
                SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

                Kernel->SetPosition(Kernel, phdr->p_offset);

                UINTN size = phdr->p_filesz;
                Kernel->Read(Kernel, &size, (void*)segment);
                break;
            }
            default:
                break;
        }
    }

    Print(L"Kernel loaded\r\n");


    PSF1Font* font = LoadPSF1Font(NULL, L"zap-light16.psf", ImageHandle, SystemTable);
    if (!font)
    {
        Print(L"Font is either invalid or not found\r\n");
        return EFI_LOAD_ERROR;
    }
    else
    {
        Print(L"Font loaded. Char size: %d\r\n", font->header->char_size);
    }

    Framebuffer* fb = InitializeGOP();

    Print(
            L"Base: 0x%lX\r\n"
            L"Size: 0x%lX\r\n"
            L"Width: %d\r\n"
            L"Height: %d\r\n"
            L"PixelsPerScanline: %d\r\n",
            fb->base_address,
            fb->size,
            fb->width,
            fb->height,
            fb->pixels_per_scanline
            );

    EFI_MEMORY_DESCRIPTOR* Map = NULL;
    UINTN MapSize, MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    {
        SystemTable->BootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
        SystemTable->BootServices->AllocatePool(EfiLoaderData, MapSize, (void**)&Map);
        SystemTable->BootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
    }

    EFI_CONFIGURATION_TABLE* config_table = SystemTable->ConfigurationTable;
    void* rsdp = NULL;
    EFI_GUID Acpi2TableGuid = ACPI_20_TABLE_GUID;

    UINTN counter = 0;

    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++)
    {
        if (CompareGuid(&config_table[i].VendorGuid, &Acpi2TableGuid))
        {
            if (strequal((CHAR8*)"RSD PTR ", (CHAR8*)config_table->VendorTable, 8))
            {
                rsdp = (void*) config_table->VendorTable;
                // TODO: Weird bug
                counter++;
                //break;
            }
        }
        config_table++;
    }

    // TODO: Weird bug: found the rsdp two times
    Print(L"Found RSDP %u times\r\n", counter);

    typedef struct BooInfo
    {
        Framebuffer* framebuffer;
        PSF1Font* font;
        struct
        {
            EFI_MEMORY_DESCRIPTOR* handle;
            UINTN size;
            UINTN descriptor_size;
        } mmap;
        void* rsdp;
    } BootInfo;

    BootInfo boot_info =
    {
        .framebuffer = &framebuffer,
        .font = font,
        .mmap = 
        {
            .handle = Map,
            .size = MapSize,
            .descriptor_size = DescriptorSize,
        },
        .rsdp = rsdp,
    };

    void (*KernelStart)(BootInfo) = ((__attribute__((sysv_abi)) void(*)(BootInfo) ) header.e_entry);

    SystemTable->BootServices->ExitBootServices(ImageHandle, MapKey);
    KernelStart(boot_info);

	return EFI_SUCCESS; // Exit the UEFI application
}
