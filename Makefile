# Сборка Cravgos без GRUB: нужны только gcc, binutils и python3
CFLAGS = -I. -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns -O2 -Wall -Wextra

all: cravgos.iso

boot_iso.o: boot_iso.s
	as --32 $< -o $@
kernel.o: kernel.c fs.h net.h http.h html.h
	gcc $(CFLAGS) -c $< -o $@
cravgos.elf: boot_iso.o kernel.o linker_iso.ld
	ld -m elf_i386 -T linker_iso.ld -o $@ boot_iso.o kernel.o
cravgos.img: cravgos.elf
	objcopy -O binary $< $@
cravgos.iso: cravgos.img mkiso.py
	python3 mkiso.py cravgos.img $@

# Альтернатива: через GRUB (нужны grub-mkrescue, xorriso)
grub: kernel.o boot.o linker.ld
	ld -m elf_i386 -T linker.ld -o cravgos.bin boot.o kernel.o
	mkdir -p isodir/boot/grub && cp cravgos.bin isodir/boot/
	printf 'set timeout=0\nmenuentry "Cravgos" {\n  multiboot /boot/cravgos.bin\n  boot\n}\n' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o cravgos-grub.iso isodir
boot.o: boot.s
	gcc -m32 -c $< -o $@

test:
	gcc -Wall -Wno-unused-function -o /tmp/test_fs tests/test_fs.c && /tmp/test_fs
	gcc -Wall -Wno-unused-function -o /tmp/test_net tests/test_net.c && /tmp/test_net
	gcc -Wall -Wno-unused-function -o /tmp/test_html tests/test_html.c && /tmp/test_html
	gcc -Wall -Wno-unused-function -fsanitize=address,undefined -I. -o /tmp/test_image tests/test_image.c && python3 tests/gen_images.py /tmp/cimg && /tmp/test_image /tmp/cimg/*
	gcc -Wall -Wno-unused-function -o /tmp/test_http tests/test_http.c && /tmp/test_http
	gcc -Wall -Wno-unused-function -o /tmp/test_crypto tests/test_crypto.c && /tmp/test_crypto
	bash tests/run_tls_tests.sh
	bash tests/run_https_e2e.sh

run: cravgos.iso
	qemu-system-i386 -cdrom cravgos.iso
clean:
	rm -rf *.o *.elf *.img cravgos.bin isodir cravgos*.iso
