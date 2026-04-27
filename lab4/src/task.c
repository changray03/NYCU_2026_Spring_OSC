#include "task.h"
#include "utils.h"
#include "uart.h"
#include "timer.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS 64
static task_t task_pool[MAX_TASKS];
static int task_used[MAX_TASKS] = {0};
static task_t *task_queue_head = NULL;
int current_task_priority = 999;

void add_task(task_callback_t callback, void *arg, int priority) {
    uint64_t s = disable_and_save_sstatus();
    task_t *new_node = NULL;
    for(int i=0; i<MAX_TASKS; i++) {
        if(!task_used[i]) {
            task_used[i] = 1;
            new_node = &task_pool[i];
            break;
        }
    }
    if(!new_node) { restore_sstatus(s); return; }

    new_node->callback = callback;
    new_node->arg = arg;
    new_node->priority = priority;
    
    // 按照優先級插入隊列 (Priority Queue)
    if (!task_queue_head || priority < task_queue_head->priority) {
        new_node->next = task_queue_head;
        task_queue_head = new_node;
    } else {
        task_t *curr = task_queue_head;
        while (curr->next && curr->next->priority <= priority) curr = curr->next;
        new_node->next = curr->next;
        curr->next = new_node;
    }
    restore_sstatus(s);
}

void run_preemptive_tasks() {
    while (1) {
        uint64_t s = disable_and_save_sstatus();
        
        if (task_queue_head == NULL || task_queue_head->priority >= current_task_priority) {
            restore_sstatus(s);
            break; 
        }

        task_t *t = task_queue_head;
        task_queue_head = t->next;
        
        task_callback_t cb = t->callback;
        void *arg = t->arg;
        int idx = t - task_pool;
        int old_priority = current_task_priority;
        current_task_priority = t->priority;

        // 開啟全域中斷執行 (可被搶佔)
        irq_enable(); 
        cb(arg);
        irq_disable(); 
        // -----------------------------------------

        // 安全區 (SIE=0)，在此解除設備屏蔽
        if (cb == (task_callback_t)uart_rx_task) {
            *UART_IER |= 0x01; // 解除 UART 屏蔽
        } else if (cb == (task_callback_t)handle_timer_interrupt_task) {
            enable_timer_interrupt(); // 解除 Timer 屏蔽
        }

        current_task_priority = old_priority;
        task_used[idx] = 0;
        restore_sstatus(s);
    }
}