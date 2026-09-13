CC = clang
AS = clang
LD = ld.lld
OBJCOPY = llvm-objcopy

TARGET = i686-elf

CFLAGS = --target=$(TARGET) -ffreestanding -O2 -Wall -Wextra -std=gnu99 -I. -Idoomgeneric
ASFLAGS = --target=$(TARGET) -c
LDFLAGS = -T linker.ld -nostdlib

SRC = kernel.c fat32.c string.c stdio.c $(wildcard doomgeneric/*.c)
OBJ = $(SRC:.c=.o)

all: kernel.bin fat32.img

initrd.tar:
	echo "Welcome to the bare-metal initrd file system!" > hello.txt
	echo "This file was loaded from a USTAR tar archive embedded into the kernel binary." > readme.txt
	tar -cf initrd.tar hello.txt readme.txt
	rm hello.txt readme.txt

initrd.o: initrd.tar
	$(OBJCOPY) -I binary -O elf32-i386 initrd.tar initrd.o

boot.o: boot.s
	$(AS) $(ASFLAGS) boot.s -o boot.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: boot.o $(OBJ) initrd.o
	$(LD) -m elf_i386 $(LDFLAGS) boot.o $(OBJ) initrd.o -o kernel.bin

fat32.img:
	dd if=/dev/zero of=fat32.img bs=1M count=64
	mkfs.fat -F 32 fat32.img
	echo "Hello from a FAT32 file!" > test.txt
	mcopy -i fat32.img test.txt ::test.txt
	rm test.txt

iso: kernel.bin grub.cfg
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os.iso isodir
	rm -rf isodir

run: kernel.bin fat32.img
	qemu-system-i386 -kernel kernel.bin -hda fat32.img -vga std

run-iso: iso fat32.img
	qemu-system-i386 -cdrom os.iso -hda fat32.img -vga std -boot d

clean:
	rm -f *.o *.bin doomgeneric/*.o initrd.tar fat32.img os.iso
