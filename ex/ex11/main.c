#define UART_BASE 0x10000000UL
#define UART_RBR  (unsigned char*)(UART_BASE + 0x0) // UART Receiver Buffer Register
#define UART_THR  (unsigned char*)(UART_BASE + 0x0) // UART Transmitter Holding Register
#define UART_LSR  (unsigned char*)(UART_BASE + 0x5) // UART Line Status Register
#define LSR_DR    (1 << 0)  // Data Ready Bit
#define LSR_TDRQ  (1 << 5)  // Transmitter Holding Register Empty Bit

char uart_getc() {
    // TODO: Implement this function
    while ((*UART_LSR & LSR_DR) == 0); // Wait until data is ready
    return *UART_RBR; // Read character from receive buffer
}

void uart_putc(char c) {
    // TODO: Implement this function
    while ((*UART_LSR & LSR_TDRQ) == 0); // Wait until transmit buffer is ready
    *UART_THR = c; // Write character to transmit buffer
}

void uart_puts(const char* s) {
    // TODO: Implement this function
    while (*s) {
        uart_putc(*s++); // Print characters until encountering end of string (\0)
    }
}

void start_kernel() {
    uart_puts("\nStarting kernel ...\n");
    while (1) {
        uart_putc(uart_getc());
    }
}
