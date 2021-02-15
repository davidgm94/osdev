all: run

rebuild: gen build

run:
	make -C build run

build:
	make -C image
gen:
	rm -rf build && mkdir build && cd build && cmake .. && cp compile_commands.json .. && cd ..
bootloader:
	make -C dependencies/gnu-efi && make -C dependencies/gnu-efi bootloader
