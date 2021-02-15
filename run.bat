set OSNAME=renaissance-os
set ROOTDIR=C:/Users/david/Documents/git/%OSNAME%
set BUILDDIR=%ROOTDIR%/build
set OVMFDIR=%ROOTDIR%/dependencies/OVMF

qemu-system-x86_64 -drive file=%BUILDDIR%/%OSNAME%.img -m 256M -cpu qemu64 -drive if=pflash,format=raw,unit=0,file=%OVMFDIR%/OVMF_CODE-pure-efi.fd,readonly=on -drive if=pflash,format=raw,unit=1,file=%OVMFDIR%/OVMF_VARS-pure-efi.fd -net none -s -S
pause