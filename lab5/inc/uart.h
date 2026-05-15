#ifndef UART_H
#define UART_H

#include <stdarg.h>
#ifdef DEBUG
    #define UART_BASE 0x10000000UL
#else
    #define UART_BASE 0xD4017000UL
#endif

#define UART_RBR  (volatile unsigned char*)(UART_BASE + 0x0)
#define UART_THR  (volatile unsigned char*)(UART_BASE + 0x0)

#ifdef DEBUG    
    #define UART_IER  (volatile unsigned char*)(UART_BASE + 0x1) // Interrupt Enable Register
    #define UART_IIR  (volatile unsigned char*)(UART_BASE + 0x2) // Interrupt Identification Register
    #define UART_MCR  (volatile unsigned char*)(UART_BASE + 0x4) // Modem Controll Register
    #define UART_LSR  (volatile unsigned char*)(UART_BASE + 0x5)
#else
    #define UART_IER  (volatile unsigned char*)(UART_BASE + 0x4) // Interrupt Enable Register
    #define UART_IIR  (volatile unsigned char*)(UART_BASE + 0x8) // Interrupt Identification Register
    #define UART_MCR  (volatile unsigned char*)(UART_BASE + 0x10) // Modem Controll Register
    #define UART_LSR  (volatile unsigned char*)(UART_BASE + 0x14)
#endif

#ifdef DEBUG
#define UART_IRQ 10
#else
#define UART_IRQ 42
#endif

#define LSR_DR    (1 << 0)
#define LSR_TDRQ  (1 << 5)
#define Rx_DR 4
#define Rx_TO 12
#define Tx_EMPTY 2

#define RING_BUFFER_SIZE 4096

struct ring_buffer {
    char buffer[RING_BUFFER_SIZE];
    int head;  // 寫入指標：指向下一個可以「放進去」的位置
    int tail;  // 讀取指標：指向下一個可以「拿出來」的位置
};

char uart_getc();
void uart_putc(char c);
void uart_puts(const char* s);
void uart_hex(unsigned long h);
void uart_decimal(unsigned long d);
void uart_printf(const char* fmt, ...);
void uart_init();
void uart_rx_task(void* arg);
void handle_uart_tx();
void uart_isr();

#endif