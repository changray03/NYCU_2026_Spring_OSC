typedef unsigned long uint64_t;
uint64_t UART_BASE;
/*
#ifdef DEBUG
    #define UART_BASE 0x10000000UL
#else
    #define UART_BASE 0xD4017000UL
#endif
*/

#define UART_RBR  (unsigned char*)(UART_BASE + 0x0)
#define UART_THR  (unsigned char*)(UART_BASE + 0x0)
#ifdef DEBUG
    #define UART_LSR  ( unsigned char*)(UART_BASE + 0x5)
#else
    #define UART_LSR  (unsigned char*)(UART_BASE + 0x14)
#endif
#define LSR_DR    (1 << 0)
#define LSR_TDRQ  (1 << 5)


char uart_getc() {
    while ((*UART_LSR & LSR_DR) == 0);
    char c = (char)*UART_RBR;
    return c == '\r' ? '\n' : c;
}

void uart_putc(char c) {
    if (c == '\n'){
        while ((*UART_LSR & LSR_TDRQ) == 0);
        *(unsigned char*)UART_THR = '\r';
    }
    while ((*UART_LSR & LSR_TDRQ) == 0);
    *(unsigned char*)UART_THR = c;
    return;
}

void uart_puts(const char* s) {
    while (*s)
        uart_putc(*s++);
}

void uart_hex(unsigned long h) {
    uart_puts("0x");
    unsigned long n;
    for (int c = 60; c >= 0; c -= 4) {
        n = (h >> c) & 0xf;
        n += n > 9 ? 0x57 : '0';
        uart_putc(n);
    }
}

void uart_decimal(unsigned long d) {
    if (d == 0) {
        uart_putc('0');
        return;
    }
    char buf[20];
    int i = 0;
    while (d > 0) {
        buf[i++] = '0' + (d % 10);
        d /= 10;
    }
    for (int j = i - 1; j >= 0; j--)
        uart_putc(buf[j]);
}

