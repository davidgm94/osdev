cmake_minimum_required(VERSION 3.16)
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
set(CMAKE_STATIC_LINKER_FLAGS rDv)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -g -F dwarf -o <OBJECT> <SOURCE>")

set(DEPENDENCIES_DIR ${CMAKE_SOURCE_DIR}/dependencies)
set(GNU_EFI_DIR ${DEPENDENCIES_DIR}/gnu-efi)
set(OVMF_DIR ${DEPENDENCIES_DIR}/OVMF)
set(KERNEL_DIR ${CMAKE_SOURCE_DIR}/src/kernel)

add_custom_target(bootloader COMMAND make -C ${GNU_EFI_DIR} && make -C ${GNU_EFI_DIR} bootloader)
add_custom_target(bl_clean COMMAND make -C ${GNU_EFI_DIR} clean)

# KERNEL LAND #

set(KERNEL_LINKER_SCRIPT ${KERNEL_DIR}/kernel.ld)
add_executable(kernel.elf 
    ${KERNEL_DIR}/acpi.c
    ${KERNEL_DIR}/keyboard.c
    ${KERNEL_DIR}/mouse.c
    ${KERNEL_DIR}/libk.c
    ${KERNEL_DIR}/panic.c
    ${KERNEL_DIR}/renderer.c
    ${KERNEL_DIR}/interrupts.c
    ${KERNEL_DIR}/gdt.nasm
    ${KERNEL_DIR}/start.nasm
    ${KERNEL_DIR}/kernel.c
    )
set_source_files_properties(${KERNEL_DIR}/interrupts.c PROPERTIES COMPILE_FLAGS -mgeneral-regs-only)
target_include_directories(kernel.elf PRIVATE ${KERNEL_DIR})
target_compile_options(kernel.elf PRIVATE -g3 -ggdb -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -fno-omit-frame-pointer)
target_link_options(kernel.elf PRIVATE -static -Bsymbolic -nostdlib -T ${KERNEL_LINKER_SCRIPT})

set(NSH_SCRIPT ${KERNEL_DIR}/startup.nsh)
set(FONT_FILE ${KERNEL_DIR}/zap-light16.psf)
set(BOOTLOADER ${GNU_EFI_DIR}/x86_64/bootloader/main.efi)

add_custom_target(image ALL COMMAND dd if=/dev/zero of=${PROJECT_NAME}.img bs=512  count=93750 && mformat -i ${PROJECT_NAME}.img -f 1440 :: && mmd -i ${PROJECT_NAME}.img ::/EFI && mmd -i ${PROJECT_NAME}.img ::/EFI/BOOT && mcopy -i ${PROJECT_NAME}.img ${BOOTLOADER} ::/EFI/BOOT && mcopy -i ${PROJECT_NAME}.img ${NSH_SCRIPT} :: && mcopy -i ${PROJECT_NAME}.img kernel.elf :: && mcopy -i ${PROJECT_NAME}.img ${FONT_FILE} :: DEPENDS kernel.elf)

add_custom_target(run
    COMMAND qemu-system-x86_64 -machine q35 -drive file=${PROJECT_NAME}.img -m 256M -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS-pure-efi.fd" -net none
    DEPENDS image)
add_custom_target(debug
    COMMAND qemu-system-x86_64 -machine q35 -drive file=${PROJECT_NAME}.img -m 256M -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="${OVMF_DIR}/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="${OVMF_DIR}/OVMF_VARS-pure-efi.fd" -net none -s -S
    DEPENDS image)

add_custom_command(TARGET kernel.elf
    PRE_BUILD
    COMMAND cp ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR})
