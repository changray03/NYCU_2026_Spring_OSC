extern char uart_getc();
extern char uart_getc_raw();
extern void uart_puts(const char* s);
extern void uart_decimal(unsigned long d);
extern void uart_hex(unsigned long h);

#ifdef DEBUG
    #define KERNEL_BASE 0x82000000UL
#else
#define KERNEL_BASE 0x20000000UL
#endif

void uart_bootloader(unsigned long hartid, unsigned long fdt) {
    char magic[4];
    for(int i = 0; i < 4; i++){
        magic[i] = uart_getc_raw();
    }
    if(magic[0] != 'B' || magic[1] != 'O' || magic[2] != 'O' || magic[3] != 'T'){
        uart_puts("Magic failed!\n");
        while(1);
    }
    uart_puts("Magic good!\n");
    
    unsigned long kernel_size = 0;
    for (int i = 0; i < 4; i++) {
        kernel_size |= ((unsigned int)uart_getc_raw() << (i * 8));
    }
    uart_puts("Kernel size: ");
    uart_decimal(kernel_size);
    uart_puts("\n");
    
    char* kernel_ptr = (char*)KERNEL_BASE;
    for (unsigned int i = 0; i < kernel_size; i++) {
        kernel_ptr[i] = uart_getc_raw();
    }
    uart_puts("Kernel loaded. Jumping...\n");
    uart_puts("Kernel address: ");
    void (*kernel_entry)(unsigned long, unsigned long) = (void (*)(unsigned long, unsigned long))KERNEL_BASE;
    uart_hex((unsigned long)kernel_entry);
    uart_puts("\n");
    kernel_entry(hartid, fdt);
}