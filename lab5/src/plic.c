#include "plic.h"
#include "uart.h"
#include "fdt.h"
#include "utils.h"
#include <stdint.h>

uint64_t plic_base;

void plic_init(uint64_t hartid, uint64_t fdt) {
    int len;
    
    #ifdef DEBUG
    int offset = fdt_path_offset((void*)fdt, "/soc/plic");
    const uint32_t* prop = fdt_getprop((const void*)fdt, offset, "reg", &len);
    if (prop){
        plic_base = (uint64_t)fdt32_to_cpu(prop) << 32 | fdt32_to_cpu(prop+1);
        uart_printf("plic base: %x\n", plic_base);
    }
    #else
    int offset = fdt_path_offset((void*)fdt, "/soc/interrupt-controller");
    const uint32_t* prop = fdt_getprop((const void*)fdt, offset, "reg", &len);
    if (prop){
        plic_base = (uint64_t)fdt32_to_cpu(prop) << 32 | fdt32_to_cpu(prop+1);
        uart_printf("plic base: %x\n", plic_base);
    }
    #endif
    
    // Set UART interrupt priority
    *(volatile unsigned int*)(PLIC_PRIORITY(UART_IRQ)) = 1;
    // Set UART interrupt enable for the boot hart
    *(volatile uint32_t *)(uintptr_t)PLIC_ENABLE(hartid, UART_IRQ) |= (1 << (UART_IRQ % 32));    // Set threshold for the boot hart
    *(volatile unsigned int*)(PLIC_THRESHOLD(hartid)) = 0;
    // Enable external interrupts
    enable_external_interrupt();
}

int plic_claim(uint64_t hartid) {
    return *(volatile unsigned int*)PLIC_CLAIM(hartid);
}

void plic_complete(int irq, uint64_t hartid) {
    *(volatile unsigned int*)PLIC_CLAIM(hartid) = irq;
}

void enable_external_interrupt() {
    asm volatile(
        "li t0, (1 << 9);"
        "csrs sie, t0;");
}