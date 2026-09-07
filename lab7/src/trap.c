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
#include "paging.h"

extern uint64_t hart_id;

void check_pending_signals(struct pt_regs *regs) {
    struct task_struct *curr = get_current();
    if (curr->pending_signals == 0) return;

    int signum = 15;
    void (*handler)() = curr->sig_handlers[signum];
    
    if (handler && (curr->pending_signals & (1 << signum))) {
        // 備份原始 U-Mode 上下文
        curr->saved_regs = allocate(sizeof(struct pt_regs));
        memcpy(curr->saved_regs, regs, sizeof(struct pt_regs));

        // 建立訊號專用棧的核心實體空間
        curr->signal_stack = (unsigned long)allocate(STACK_SIZE);
        memzero((void*)curr->signal_stack, STACK_SIZE);
        
        // 開闢一個安全的低位元區段 0x50000000，並賦予最高權限（RWX + User）
        unsigned long sig_stack_pa = virt_to_phys(curr->signal_stack);
        map_pages(0x50000000, STACK_SIZE, sig_stack_pa, PROT_USER_RW | PTE_X); 

        // 在新鋪好路的記憶體開頭寫入 Trampoline 指令
        uint32_t *tramp = (uint32_t *)curr->signal_stack;
        tramp[0] = 0x00b00893; // li a7, 11 (SYS_SIGRETURN)
        tramp[1] = 0x00000073; // ecall

        // 強制刷清指令快取！確保 U-Mode 的 I-Cache 100% 認得這兩條剛剛織好的新指令
        asm volatile("fence.i");

        // 修改暫存器進入 Handler
        regs->epc = (unsigned long)handler;
        
        // 交給 U-Mode 暫存器的，必須是低位元的使用者虛擬座標！
        regs->ra = 0x50000000;              // 讓 handler 執行完 ret 時，精準跳到使用者空間的 Trampoline
        regs->sp = 0x50000000 + STACK_SIZE; // 讓核心處理訊號時，擁有獨立合法的使用者棧頂

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
            
            case 12:
            case 13:
            case 15:
                page_fault_handler(regs, cause_num);
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