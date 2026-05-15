#ifndef PLIC_H
#define PLIC_H
#include <stdint.h>
//#define PLIC_BASE            0xc000000UL
extern uint64_t plic_base;

#define PLIC_PRIORITY(irq)   (plic_base + (irq) * 4)
// 考慮到 IRQ 超過 31 的情況，需要根據 IRQ 增加偏移量
#define PLIC_ENABLE(hart, irq) (plic_base + 0x002080 + (hart) * 0x0100 + ((irq) / 32) * 4)
#define PLIC_THRESHOLD(hart) (plic_base + 0x201000 + (hart) * 0x2000)
#define PLIC_CLAIM(hart)     (plic_base + 0x201004 + (hart) * 0x2000)

void plic_init(uint64_t hartid, uint64_t fdt);
int plic_claim(uint64_t hartid);
void plic_complete(int irq, uint64_t hartid);
void enable_external_interrupt();

#endif