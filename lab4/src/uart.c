#include "uart.h"
#include <stdint.h>
#include "utils.h"
#include "task.h"

struct ring_buffer rx_buf = {.head = 0, .tail = 0};
struct ring_buffer tx_buf = {.head = 0, .tail = 0};

static int is_buf_empty(struct ring_buffer *rb);
static int is_buf_full(struct ring_buffer *rb);
extern void handle_shell_input(char c);

char uart_getc() {
    char c;
    while (1) {
        uint64_t s = disable_and_save_sstatus();
        if (!is_buf_empty(&rx_buf)) {
            c = rx_buf.buffer[rx_buf.tail];
            rx_buf.tail = (rx_buf.tail + 1) % RING_BUFFER_SIZE;
            restore_sstatus(s);
            return c;
        }
        restore_sstatus(s);
        asm volatile("wfi"); // 沒字就睡覺，等 RX 中斷喚醒
    }
}

void uart_putc(char c) {
    if(c == '\n') uart_putc('\r');
    uint64_t s = disable_and_save_sstatus();
    // 如果 Buffer 滿了，就等它被中斷清出空間
    while (is_buf_full(&tx_buf)) {
        restore_sstatus(s);
        asm volatile("wfi");
        s = disable_and_save_sstatus();
    }

    tx_buf.buffer[tx_buf.head] = c;
    tx_buf.head = (tx_buf.head + 1) % RING_BUFFER_SIZE;

    // 開啟 UART 的 TX 中斷 (IER bit 1)，讓 ISR 開始搬運資料
    *UART_IER |= 0x02; 
    
    restore_sstatus(s);
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

void uart_init() {
    // TODO: Enable RX interrupt
    *UART_MCR |= 1 << 3;
    // TODO: Enable UART interrupt
    *UART_IER |= 1;
}

void handle_uart_rx() {
    // 只要 UART 還有資料 (LSR bit 0 == 1)，就一直讀
    while (*UART_LSR & LSR_DR) {
        char c = *UART_RBR;
        if (!is_buf_full(&rx_buf)) {
            rx_buf.buffer[rx_buf.head] = c;
            rx_buf.head = (rx_buf.head + 1) % RING_BUFFER_SIZE;
        }
    }
}

void handle_uart_tx() {
    // 條件：只要軟體 Buffer 還有東西，且硬體 FIFO 還能塞字元 (LSR_TDRQ 為 1)
    while (!is_buf_empty(&tx_buf) && (*UART_LSR & LSR_TDRQ)) {
        char c = tx_buf.buffer[tx_buf.tail];
        tx_buf.tail = (tx_buf.tail + 1) % RING_BUFFER_SIZE;
        *UART_THR = c;
    }

    // 當軟體 Buffer 沒東西了，才關閉 TX 中斷
    if (is_buf_empty(&tx_buf)) {
        *UART_IER &= ~0x02; // 防止無限進入中斷
    }
}

static int is_buf_empty(struct ring_buffer *rb) {
    return rb->head == rb->tail;
}

static int is_buf_full(struct ring_buffer *rb) {
    return ((rb->head + 1) % RING_BUFFER_SIZE) == rb->tail;
}

void uart_rx_task(void *arg) {
    // 直接傳入字元值（透過 pointer casting）
    char c = (char)(uintptr_t)arg;

    // 處理 Shell 緩衝區邏輯
    handle_shell_input(c); 

    //*UART_IER |= 0x01; 
}

void uart_isr() {
    while (1) {
        uint8_t iir = *UART_IIR;
        if (iir & 0x01) break; 

        uint8_t reason = iir & 0x0E;
        if (reason == 0x04 || reason == 0x0C) {
            // Top half
            char c = *UART_RBR;

            // 屏蔽 UART 中斷 (防止 UART 任務被自己打斷)
            *UART_IER &= ~0x01; 

            // 將處理邏輯作為 Task 加入隊列，優先級設為 1 (高)
            add_task(uart_rx_task, (void *)(uintptr_t)c, 1);
        } 
        else if (reason == 0x02) {
            // top half
            handle_uart_tx();
        }
    }
}