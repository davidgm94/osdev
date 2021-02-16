all: run

rebuild: gen build

run:
	make -C build run
debug:
	make -C build debug

build:
	make -C build image
gen:
	rm -rf build && mkdir build && cd build && cmake .. && cp compile_commands.json .. && cd ..
bootloader:
	make -C dependencies/gnu-efi && make -C dependencies/gnu-efi bootloader
