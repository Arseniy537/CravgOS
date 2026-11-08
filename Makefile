# Сборка Cravgos x86_64: нужны gcc, binutils и python
CFLAGS = -I. -m64 -mgeneral-regs-only -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -mno-stack-arg-probe -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-exceptions -fno-tree-loop-distribute-patterns -fno-optimize-sibling-calls -O2 -Wall -Wextra

ifeq ($(OS),Windows_NT)
    PYTHON ?= python
    LDFLAGS ?= --image-base 0x0
    RM ?= del /f /q 2>nul || exit 0
else
    PYTHON ?= python3
    LDFLAGS ?= -m elf_x86_64
    RM ?= rm -rf
endif

all: bootx64.efi cravgos.iso cravgos.bin

bootx64.efi: boot_efi.c
	gcc -nostdlib -m64 -O2 "-Wl,--subsystem,10" -e efi_main $< -o $@

boot_iso.o: boot_iso.s cyrfont.inc
	as $< -o $@
kernel.o: kernel.c lua.h stas.h stas_lua_pkgs.h fs.h net.h http.h html.h crypto.h tls.h image.h cyrfont.h
	gcc $(CFLAGS) -c $< -o $@
# Lua-пакеты из stas-repo/ встраиваются в ядро для офлайн-режима (stas repo offline)
stas_lua_pkgs.h: tools/stas_embed.py $(wildcard stas-repo/*.stp)
	$(PYTHON) tools/stas_embed.py
# после правок пакетов stas: пересобрать index.txt и встраиваемые копии
repo:
	$(PYTHON) tools/stas_mkrepo.py stas-repo
	$(PYTHON) tools/stas_embed.py
cravgos.elf: boot_iso.o kernel.o linker_iso.ld
	ld $(LDFLAGS) -T linker_iso.ld -o $@ boot_iso.o kernel.o
cravgos.img: cravgos.elf
	objcopy -O binary -R .bigbss -R .reloc $< $@
cravgos.iso: cravgos.img mkiso.py bootx64.efi
	$(PYTHON) mkiso.py cravgos.img $@

# Альтернатива: через GRUB (нужны grub-mkrescue, xorriso)
cravgos.bin: boot.o kernel.o linker.ld
	ld $(LDFLAGS) -T linker.ld -o $@ boot.o kernel.o
grub: cravgos.bin
	mkdir -p isodir/boot/grub && cp cravgos.bin isodir/boot/
	printf 'set timeout=0\nmenuentry "Cravgos (x86_64)" {\n  multiboot /boot/cravgos.bin\n  boot\n}\n' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o cravgos-grub.iso isodir
boot.o: boot.s
	as $< -o $@

test:
	$(PYTHON) tests/run_all.py

run: cravgos.iso
	qemu-system-x86_64 -cdrom cravgos.iso

clean:
	$(PYTHON) -c "import os, glob; [os.remove(f) for p in ('*.o','*.elf','*.img','*.bin','*.efi','cravgos*.iso') for f in glob.glob(p)]"
