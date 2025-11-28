#!/bin/bash

# Clean old builds
rm -f kasm.o kc.o kernel ZurOS.iso

# Assemble kernel
nasm -f elf32 kernel.asm -o kasm.o

# Compile C kernel
gcc -m32 -ffreestanding -c kernel.c -o kc.o

# Link everything
ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o

# Copy kernel into ISO structure
cp kernel iso/boot/kernel

# Create ISO (for bootloader)
grub-mkrescue -o ZurOS.iso iso

# Create persistent HDD if missing
if [ ! -f hdd.img ]; then
    echo "Creating 64MB FAT32 disk image (hdd.img)..."
    qemu-img create -f raw hdd.img 64M
    mkfs.fat -F 32 hdd.img
fi

# Run QEMU with:
#  - CD-ROM for booting
#  - HDD for FAT32 persistent storage
qemu-system-i386 \
    -cdrom ZurOS.iso \
    -drive file=hdd.img,format=raw,index=0,media=disk \
    -boot d
