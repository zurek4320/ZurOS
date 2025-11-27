#!/bin/bash

# Clean old builds
rm -f kasm.o kc.o kernel myos.iso

# Assemble kernel
nasm -f elf32 kernel.asm -o kasm.o

# Compile C kernel
gcc -m32 -ffreestanding -c kernel.c -o kc.o

# Link everything
ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o

cp kernel iso/boot/kernel

# Create ISO for GRUB
grub-mkrescue -o ZurOS.iso iso

# Boot in QEMU
qemu-system-i386 -cdrom ZurOS.iso
