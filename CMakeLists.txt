cmake_minimum_required(VERSION 3.15)
set(PROJ_NAME renaissance-os)
project(${PROJ_NAME} C)
#enable_language(ASM)
enable_language(ASM_NASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_AUTOGEN_VERBOSE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_LINKER ld)
#set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES> /usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/libgcc.a")
set(CMAKE_STATIC_LINKER_FLAGS rDv)
#set(CMAKE_C_CREATE_STATIC_LIBRARY "<CMAKE_AR> <LINK_FLAGS> <TARGET> <OBJECTS>")
#set(ARCH x86_64)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")

set(DEPENDENCIES_DIR ${CMAKE_SOURCE_DIR}/dependencies)
set(GNU_EFI_DIR ${DEPENDENCIES_DIR}/gnu-efi)
set(OVMF_DIR ${DEPENDENCIES_DIR}/OVMF)
set(KERNEL_DIR ${CMAKE_SOURCE_DIR}/src/kernel)

#set(GNU_EFI_INCLUDE_DIRECTORIES ${GNU_EFI_DIR}/inc ${GNU_EFI_DIR}/inc/${ARCH} ${GNU_EFI_DIR}/inc/protocol)
#set(GNU_EFI_COMPILE_DEFINITIONS CONFIG_x86_64=1)
#set(GNU_EFI_COMPILE_OPTIONS -Wno-error=pragmas -mno-red-zone -m64 -mno-avx -fpic -g -O2 -Wall -Wextra -Werror -fshort-wchar -fno-strict-aliasing -ffreestanding -fno-stack-protector -fno-stack-check -fno-merge-all-constants)
#set(GNU_EFI_LINK_OPTIONS -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1)

## libefi

#set(LIBEFI_DIR ${GNU_EFI_DIR}/lib)
#add_library(efi
    #${LIBEFI_DIR}/boxdraw.c
    #${LIBEFI_DIR}/smbios.c
    #${LIBEFI_DIR}/console.c
    #${LIBEFI_DIR}/crc.c
    #${LIBEFI_DIR}/data.c
    #${LIBEFI_DIR}/debug.c
    #${LIBEFI_DIR}/dpath.c
    #${LIBEFI_DIR}/error.c
    #${LIBEFI_DIR}/event.c
    #${LIBEFI_DIR}/exit.c
    #${LIBEFI_DIR}/guid.c
    #${LIBEFI_DIR}/hand.c
    #${LIBEFI_DIR}/hw.c
    #${LIBEFI_DIR}/init.c
    #${LIBEFI_DIR}/lock.c
    #${LIBEFI_DIR}/misc.c
    #${LIBEFI_DIR}/print.c
    #${LIBEFI_DIR}/sread.c
    #${LIBEFI_DIR}/str.c
    #${LIBEFI_DIR}/cmdline.c
    #${LIBEFI_DIR}/runtime/rtlock.c
    #${LIBEFI_DIR}/runtime/efirtlib.c
    #${LIBEFI_DIR}/runtime/rtstr.c
    #${LIBEFI_DIR}/runtime/vm.c
    #${LIBEFI_DIR}/runtime/rtdata.c
    #${LIBEFI_DIR}/${ARCH}/initplat.c
    #${LIBEFI_DIR}/${ARCH}/math.c
    #${LIBEFI_DIR}/${ARCH}/setjmp.S
    #${LIBEFI_DIR}/${ARCH}/callwrap.c
    #${LIBEFI_DIR}/${ARCH}/efi_stub.S)

#target_include_directories(efi PRIVATE ${GNU_EFI_INCLUDE_DIRECTORIES} ${LIBEFI_DIR})
#target_compile_definitions(efi PRIVATE ${GNU_EFI_COMPILE_DEFINITIONS})
#target_compile_options(efi PRIVATE ${GNU_EFI_COMPILE_OPTIONS})

## libgnuefi

#set(LIBGNUEFI_DIR ${GNU_EFI_DIR}/gnuefi)
#set(GNU_EFI_SRC ${LIBGNUEFI_DIR}/reloc_${ARCH}.c)

#add_library(gnuefi ${GNU_EFI_SRC})

#target_include_directories(gnuefi PRIVATE ${GNU_EFI_INCLUDE_DIRECTORIES})
#target_compile_options(gnuefi PRIVATE ${GNU_EFI_COMPILE_OPTIONS})
#target_compile_definitions(gnuefi PRIVATE ${GNU_EFI_COMPILE_DEFINITIONS})

## bootloader

#set(BOOTLOADER main.so)
#set(BOOTLOADER_DIR ${GNU_EFI_DIR}/bootloader)
#set(LINUX_HEADERS /usr/src/sys/build)
#set(LDSCRIPT ${LIBGNUEFI_DIR}/elf_${ARCH}_efi.lds)

#add_executable(${BOOTLOADER} ${BOOTLOADER_DIR}/main.c ${LIBGNUEFI_DIR}/crt0-efi-${ARCH}.S)

#target_include_directories(${BOOTLOADER} PRIVATE ${GNU_EFI_INCLUDE_DIRECTORIES} ${LINUX_HEADERS}/include)
#target_compile_options(${BOOTLOADER} PRIVATE ${GNU_EFI_COMPILE_OPTIONS})
#target_compile_definitions(${BOOTLOADER} PRIVATE ${GNU_EFI_COMPILE_DEFINITIONS} KERNEL=1)
#target_link_options(${BOOTLOADER} PRIVATE ${GNU_EFI_LINK_OPTIONS} -shared -Bsymbolic -T ${LDSCRIPT})

#execute_process(COMMAND ${CMAKE_C_COMPILER} -m64 --print-libgcc-file-name OUTPUT_VARIABLE LIBGCC_FILENAME RESULT_VARIABLE LIBGCC_RET OUTPUT_STRIP_TRAILING_WHITESPACE)
#if (${LIBGCC_RET} EQUAL 0)
    #string(STRIP "${LIBGCC_FILENAME}" LIBGCC_FILENAME)
    #file(TO_CMAKE_PATH "${LIBGCC_FILENAME}" LIBGCC_FILENAME)
    #get_filename_component(LIBGCC_DIR ${LIBGCC_FILENAME} DIRECTORY)
    #message("Found libgcc at ${LIBGCC_FILENAME}")
    #message("Directory: ${LIBGCC_DIR}")
    #target_link_directories(${BOOTLOADER} PRIVATE ${LIBGCC_DIR})
    #target_link_libraries(${BOOTLOADER} PRIVATE efi gnuefi)
#else()
    #message(FATAL_ERROR "Can't find libgcc ${LIBGCC_FILENAME} ${LIBGCC_DIR}")
#endif()

## main.efi

#add_custom_target(main.efi
    #COMMAND objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* -j .reloc --target efi-app-x86_64 main.so main.efi
    #DEPENDS ${BOOTLOADER})



add_custom_target(bootloader COMMAND make -C ${GNU_EFI_DIR} && make -C ${GNU_EFI_DIR} bootloader)
add_custom_target(bl_clean COMMAND make -C ${GNU_EFI_DIR} clean)

# KERNEL LAND #

set(KERNEL_LINKER_SCRIPT ${KERNEL_DIR}/kernel.ld)
add_executable(kernel.elf 
    ${KERNEL_DIR}/interrupts.c
    ${KERNEL_DIR}/gdt.nasm
    ${KERNEL_DIR}/kernel.c
    )
set_source_files_properties(${KERNEL_DIR}/interrupts.c PROPERTIES COMPILE_FLAGS -mgeneral-regs-only)
target_include_directories(kernel.elf PRIVATE ${KERNEL_DIR})
target_compile_options(kernel.elf PRIVATE -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector)
target_link_options(kernel.elf PRIVATE -static -Bsymbolic -nostdlib -T ${KERNEL_LINKER_SCRIPT})

set(NSH_SCRIPT ${KERNEL_DIR}/startup.nsh)
set(FONT_FILE ${KERNEL_DIR}/zap-light16.psf)
set(BOOTLOADER ${GNU_EFI_DIR}/bootloader/main.efi)

add_custom_target(image ALL COMMAND dd if=/dev/zero of=${PROJECT_NAME}.img bs=512 count=93750 && mformat -i ${PROJECT_NAME}.img -f 1440 :: && mmd -i ${PROJECT_NAME}.img ::/EFI && mmd -i ${PROJECT_NAME}.img ::/EFI/BOOT && mcopy -i ${PROJECT_NAME}.img ${BOOTLOADER} ::/EFI/BOOT && mcopy -i ${PROJECT_NAME}.img ${NSH_SCRIPT} :: && mcopy -i ${PROJECT_NAME}.img kernel.elf :: && mcopy -i ${PROJECT_NAME}.img ${FONT_FILE} :: DEPENDS kernel.elf)

add_custom_target(run
    COMMAND qemu-system-x86_64 -drive file=${PROJECT_NAME}.img -m 256M -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS-pure-efi.fd" -net none
    DEPENDS image)

add_custom_command(TARGET kernel.elf
    PRE_BUILD
    COMMAND cp ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR})
