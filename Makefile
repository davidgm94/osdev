all: run

rebuild: gen build

run:
	make -C build run

build:
	make -C build
gen:
	rm -rf build && mkdir build && pushd build && cmake .. && cp compile_commands.json .. && popd
bootloader:
	make -C dependencies/gnu-efi && make -C dependencies/gnu-efi bootloader
