OUTPUT_ARCH(riscv)
ENTRY(_start)   /* Set the entry point of kernel*/
SECTIONS
{
    . = 0x80200000;
    _start = .;
    .text : ALIGN(16) { *(.text.boot) *(.text) }
    .rodata : ALIGN(16) { *(.rodata) }
    .data : ALIGN(16) { *(.data) }
    .bss : ALIGN(16) {
        __bss_start = .;
        *(.sbss) *(.bss)
        __bss_stop = .;
    }
    . = ALIGN(16);
    . = . + 0x4000;
    _end = .;
}
