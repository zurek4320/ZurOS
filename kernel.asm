section .multiboot_header
    align 4
    dd 0x1BADB002           ; random number I NEED TO ADD
    dd 0x00                 ; flags
    dd -(0x1BADB002 + 0x00) ; checksum

; Kernel entry point
section .text
global start
extern kmain

start:
    cli             ; disable interrupts
    call kmain      ; jump to C kernel
    hlt             ; halt CPU if kmain returns
