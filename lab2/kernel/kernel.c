typedef unsigned char      uint8_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;
typedef unsigned long      uintptr_t;
typedef unsigned long      size_t;
#define NULL ((void*)0)

extern uint64_t UART_BASE;
extern char uart_getc();
extern void uart_putc(char c);
extern void uart_puts(const char* s);
extern void uart_hex(unsigned long h);
extern void uart_decimal(unsigned long d);
extern int fdt_path_offset(const void *fdt, const char *path);
extern const void *fdt_getprop(const void *fdt, int nodeoffset, const char *name, int *lenp);
extern void cpio_ls(void *archive);
extern void cpio_cat(void *archive, const char *target_name);

/* 安全讀取大端序 32-bit (解決 Alignment Trap) */
static uint32_t fdt32_to_cpu(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(uint8_t *)s1 - *(uint8_t *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    return n == 0 ? 0 : (*(uint8_t *)s1 - *(uint8_t *)s2);
}



void shell(void *cpio_base) {
    char buf[128];

    uart_puts("Welcome to OSC Shell!\n\n\n");

    while (1) {
        uart_puts("# ");
        int i = 0;
        
        while (1) {
            char c = uart_getc();
            if(c == 0x1B){
                uart_getc();
                uart_getc();
                continue;
            }
            if(c == '\r' || c == '\n'){
                uart_putc('\n');
                buf[i] = '\0';
                break;
            }
            else if(c == '\b' || c == 127){
                if(i > 0){
                    uart_putc('\b');
                    uart_putc(' ');
                    uart_putc('\b');
                    i--;
                }
            }
            else{
                if(i < 127 && c >= 32 && c <= 126){
                    uart_putc(c);
                    buf[i++] = c;
                }
            }
        }

        // 處理指令
        if (strcmp(buf, "ls") == 0) {
            cpio_ls(cpio_base);
        } else if (strncmp(buf, "cat ", 4) == 0) {
            cpio_cat(cpio_base, buf + 4); 
        } else if (buf[0] != '\0') {
            uart_puts("Unknown command: ");
            uart_puts(buf);
            uart_puts("\n");
        }
    }
}

void start_kernel(uint64_t hartid,  uint64_t fdt) {
    /*
    #ifdef DEBUG
        UART_BASE = 0x10000000;
    #else
        UART_BASE = 0xD4017000;
    #endif

    uint64_t uart_detect_base = 0;
    uart_puts("\n----------Kernel Running----------\n");
    uart_puts("\n");
    */
    // QEMU 模擬器路徑通常為 /soc/uart，Orange Pi RV2 為 /soc/serial
    int offset = offset = fdt_path_offset((void *)fdt, "/soc/serial");
    if (offset < 0) {
        fdt_path_offset((void *)fdt, "/soc/uart");
    }

    if (offset >= 0) {
        int len;
        const uint32_t *reg = fdt_getprop((void *)fdt, offset, "reg", &len);
        
        if (reg) {
            uint64_t addr_h = fdt32_to_cpu(&reg[0]);
            uint64_t addr_l = fdt32_to_cpu(&reg[1]);
            
            UART_BASE = (addr_h << 32) | addr_l;

            uart_puts("Found UART base from Devicetree: ");
            uart_hex(UART_BASE);
            uart_puts("\n");
        } else {
            uart_puts("Error: 'reg' property not found.\n");
        }
    } else {
        uart_puts("Error: UART node not found in Devicetree.\n");
    }

    uart_puts("Devicetree parsing complete. Starting Shell...\n");

    void *cpio_base = NULL;
    int chosen_off = fdt_path_offset((void *)fdt, "/chosen");
    if (chosen_off >= 0) {
        int len;
        const uint32_t *start_ptr = fdt_getprop((void *)fdt, chosen_off, "linux,initrd-start", &len);
        if (start_ptr) {
            if (len == 4) {
                cpio_base = (void *)(uintptr_t)fdt32_to_cpu(start_ptr);
            } else {
                cpio_base = (void *)(((uint64_t)fdt32_to_cpu(start_ptr) << 32) | fdt32_to_cpu(start_ptr + 1));
            }
        }
    }
    
    if (cpio_base) {
        uart_puts("Initrd found at: ");
        uart_hex((unsigned long)cpio_base);
        uart_puts("\n");
        
        shell(cpio_base);
    } else {
        uart_puts("Error: Could not find initrd in devicetree.\n");
    }
    
    while (1);
}