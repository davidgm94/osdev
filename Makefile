all:
	pushd gnu-efi && make && make bootloader && popd
	pushd kernel && make && make buildimg && make run
