# Сборка Cravgos x86_64: нужны gcc, binutils и python
CFLAGS = -I. -m64 -mgeneral-regs-only -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -mno-stack-arg-probe -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-exceptions -fno-tree-loop-distribute-patterns -O2 -Wall -Wextra

ifeq ($(OS),Windows_NT)
    PYTHON ?= python
    LDFLAGS ?= --image-base 0x0
    RM ?= del /f /q 2>nul || exit 0
else
    PYTHON ?= python3
    LDFLAGS ?= -m elf_x86_64
    RM ?= rm -rf
endif

all: cravgos.iso

boot_iso.o: boot_iso.s
	as $< -o $@
kernel.o: kernel.c fs.h net.h http.h html.h crypto.h tls.h image.h cyrfont.h
	gcc $(CFLAGS) -c $< -o $@
cravgos.elf: boot_iso.o kernel.o linker_iso.ld
	ld $(LDFLAGS) -T linker_iso.ld -o $@ boot_iso.o kernel.o
cravgos.img: cravgos.elf
	objcopy -O binary -R .bigbss -R .reloc $< $@
cravgos.iso: cravgos.img mkiso.py
	$(PYTHON) mkiso.py cravgos.img $@

# Альтернатива: через GRUB (нужны grub-mkrescue, xorriso)
grub: kernel.o boot.o linker.ld
	ld $(LDFLAGS) -T linker.ld -o cravgos.bin boot.o kernel.o
	mkdir -p isodir/boot/grub && cp cravgos.bin isodir/boot/
	printf 'set timeout=0\nmenuentry "Cravgos (x86_64)" {\n  multiboot /boot/cravgos.bin\n  boot\n}\n' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o cravgos-grub.iso isodir
boot.o: boot.s
	as $< -o $@

test:
	gcc -Wall -Wno-unused-function -I. -o /tmp/test_fs tests/test_fs.c && /tmp/test_fs
	gcc -Wall -Wno-unused-function -I. -o /tmp/test_net tests/test_net.c && /tmp/test_net
	gcc -Wall -Wno-unused-function -I. -o /tmp/test_html tests/test_html.c && /tmp/test_html
	gcc -Wall -Wno-unused-function -fsanitize=address,undefined -I. -o /tmp/test_image tests/test_image.c && $(PYTHON) tests/gen_images.py /tmp/cimg && /tmp/test_image /tmp/cimg/*
	gcc -Wall -Wno-unused-function -I. -o /tmp/test_http tests/test_http.c && /tmp/test_http
	gcc -Wall -Wno-unused-function -I. -o /tmp/test_crypto tests/test_crypto.c && /tmp/test_crypto
	bash tests/run_tls_tests.sh
	bash tests/run_https_e2e.sh

run: cravgos.iso
	qemu-system-x86_64 -cdrom cravgos.iso

clean:
	$(PYTHON) -c "import os, glob; [os.remove(f) for p in ('*.o','*.elf','*.img','cravgos.bin','cravgos*.iso') for f in glob.glob(p)]"
