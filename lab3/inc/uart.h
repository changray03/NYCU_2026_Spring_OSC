#include <stdarg.h>
#ifdef DEBUG
    #define UART_BASE 0x10000000UL
#else
    #define UART_BASE 0xD4017000UL
#endif

#define UART_RBR  (unsigned char*)(UART_BASE + 0x0)
#define UART_THR  (unsigned char*)(UART_BASE + 0x0)
#ifdef DEBUG
    #define UART_LSR  ( unsigned char*)(UART_BASE + 0x5)
#else
    #define UART_LSR  (unsigned char*)(UART_BASE + 0x14)
#endif

#define LSR_DR    (1 << 0)
#define LSR_TDRQ  (1 << 5)

char uart_getc();
void uart_putc(char c);
void uart_puts(const char* s);
void uart_hex(unsigned long h);
void uart_decimal(unsigned long d);
void uart_printf(const char* fmt, ...);
