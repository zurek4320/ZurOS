ENTRY(start)

SECTIONS
{
    /* load kernel at 1MB */
    . = 1M;

    /* Multiboot header must be first */
    .multiboot_header ALIGN(4K) : { *(.multiboot_header) }

    /* code section */
    .text ALIGN(4K) : { *(.text) }

    /* read-only data */
    .rodata ALIGN(4K) : { *(.rodata) }

    /* data section */
    .data ALIGN(4K) : { *(.data) }

    /* BSS (uninitialized data) */
    .bss ALIGN(4K) : { *(.bss COMMON) }
}
