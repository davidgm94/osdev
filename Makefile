all:
	pushd gnu-efi && make bootloader && popd
	pushd kernel && make && make buildimg && make run
