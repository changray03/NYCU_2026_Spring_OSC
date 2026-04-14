#include "uart.h"

char uart_getc() {
    while ((*UART_LSR & LSR_DR) == 0);
    char c = (char)*UART_RBR;
    return c == '\r' ? '\n' : c;
}

void uart_putc(char c) {
    if (c == '\n'){
        while ((*UART_LSR & LSR_TDRQ) == 0);
        *UART_THR = '\r';
    }
    while ((*UART_LSR & LSR_TDRQ) == 0);
    *UART_THR = c;
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

void uart_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        // 如果不是 %，直接輸出字元
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }

        // 處理 % 後面的格式化字元
        p++; 
        switch (*p) {
            case 'c': // 字元
                uart_putc((char)va_arg(args, int));
                break;

            case 's': // 字串
                uart_puts(va_arg(args, char*));
                break;

            case 'd': { // 有號整數
                long d = va_arg(args, long);
                if (d < 0) {
                    uart_putc('-');
                    d = -d;
                }
                uart_decimal((unsigned long)d);
                break;
            }

            case 'u': // 無號整數
                uart_decimal(va_arg(args, unsigned long));
                break;

            case 'x': // 十六進制
                uart_hex(va_arg(args, unsigned long));
                break;

            case 'p': // 指標位址
                uart_hex(va_arg(args, unsigned long));
                break;

            case '%': // 輸出 % 本身
                uart_putc('%');
                break;

            default: // 不支援的格式，印出原樣
                uart_putc('%');
                uart_putc(*p);
                break;
        }
    }

    va_end(args);
}
