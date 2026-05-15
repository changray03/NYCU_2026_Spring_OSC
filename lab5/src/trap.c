#include "trap.h"
#include "timer.h"
#include "uart.h"
#include <stddef.h>
#include <task.h>
#include "plic.h"
#include "syscall.h"
#include "utils.h"
#include "sched.h"
#include "memory.h"

extern uint64_t hart_id;

void check_pending_signals(struct pt_regs *regs) {
    struct task_struct *curr = get_current();
    if (curr->pending_signals == 0) return;

    // 取得第一個 pending 的訊號
    int signum = 15; 
    void (*handler)() = curr->sig_handlers[signum];
    
    if (handler) {
        // 備份上下文
        curr->saved_regs = allocate(sizeof(struct pt_regs));
        memcpy(curr->saved_regs, regs, sizeof(struct pt_regs));

        // 建立訊號專用棧
        curr->signal_stack = (unsigned long)allocate(STACK_SIZE);
        
        // 在棧的開頭寫入 Trampoline 指令
        // 注意：這裡假設 User 頁面具有執行權限
        uint32_t *tramp = (uint32_t *)curr->signal_stack;
        tramp[0] = 0x00b00893; // li a7, 11
        tramp[1] = 0x00000073; // ecall

        // 修改暫存器進入 Handler
        regs->epc = (unsigned long)handler;
        regs->ra = curr->signal_stack; // 指向我們剛寫好的指令
        regs->sp = curr->signal_stack + STACK_SIZE; // 指向棧頂

        // 清除標記
        curr->pending_signals &= ~(1 << signum);
    }
}

void do_trap(struct pt_regs* regs) {
    int is_interrupt = (regs->scause & (1UL << 63)) ? 1 : 0;
    unsigned long cause_num = regs->scause & 0xFFF;

    if (is_interrupt) {
        switch (cause_num) {
            case 5: // Timer
                //uart_printf("ticks\n");
                sbi_set_timer(get_ticks() + cpu_freq/32);
                schedule();
                break;
            case 9: { // External
                int irq = plic_claim(hart_id);
                if (irq == UART_IRQ) uart_isr();
                if (irq) plic_complete(irq, hart_id);
                break;
            }
        }
        run_preemptive_tasks();
    } else {
        // --- 處理異常 (Exception) ---
        switch (cause_num) {
            case 8: // Environment call from U-mode (ecall)
                irq_enable();
                handle_syscall(regs);
                irq_disable();
                break;
            default:
                irq_enable();
                uart_printf("Exception occurred! cause: %d, epc: 0x%x\n", cause_num, regs->epc);
                irq_disable();
                while(1);
        }
    }
    if ((regs->sstatus & (1 << 8)) == 0) {
        // 只有回 User Mode 之前才處理訊號
        check_pending_signals(regs);
    }
}