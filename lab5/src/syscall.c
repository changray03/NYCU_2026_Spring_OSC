#include "syscall.h"
#include <stdint.h>
#include <stddef.h>
#include "trap.h"
#include "uart.h"
#include "sched.h"
#include "utils.h"
#include "memory.h"
#include "timer.h"

extern int do_exec(struct task_struct *task, const char *filename);
extern void video_bmp_display(unsigned int* bmp_image, int width, int height);

void handle_syscall(struct pt_regs *regs) {
    int syscall_num = regs->a7;
    int skip_epc_add = 0;
    switch (syscall_num) {
        case SYS_GETPID:
            regs->a0 = sys_getpid();
            break;
        case SYS_UART_READ:
            regs->a0 = sys_uart_read((char *)regs->a0, (size_t)regs->a1);
            break;
        case SYS_UART_WRITE:
            regs->a0 = sys_uart_write((const char *)regs->a0, (size_t)regs->a1);
            break;
        case SYS_EXEC:
            regs->a0 = sys_exec((char *)regs->a0);
            if(regs->a0 != -1)skip_epc_add = 1; // exec 啟動新程式，不需要跳過 ecall 指令
            break;
        case SYS_FORK:
            regs->a0 = sys_fork(regs);
            break;
        case SYS_WAITPID:
            regs->a0 = sys_waitpid((int)regs->a0);
            break;
        case SYS_EXIT:
            sys_exit();
            return;
        case SYS_STOP:
            regs->a0 = sys_stop((int)regs->a0);
            break;
        case SYS_DISPLAY:
            // a0: bmp_image, a1: width, a2: height
            sys_video_bmp_display((unsigned int*)regs->a0, (unsigned int)regs->a1, (unsigned int)regs->a2);
            break;

        case SYS_USLEEP:
            // a0: usec
            regs->a0 = sys_usleep((unsigned int)regs->a0); // 回傳值存回 a0
            break;

        case SYS_SIGNAL:
            regs->a0 = sys_signal((int)regs->a0, (void (*)())regs->a1);
            break;
        case SYS_SIGRETURN:
            sys_sigreturn(regs);
            skip_epc_add = 1; // 還原後 epc 已經是原本的位置，不需要加 4
            break;
        case SYS_KILL:
            regs->a0 = sys_kill((int)regs->a0, (int)regs->a1);
            break;
            
        default:
            uart_puts("Unknown syscall\n");
            break;
    }
    if (!skip_epc_add) {
        regs->epc += 4; // 正常跳過 ecall
    }
    //check_pending_signals(regs);
}

uint64_t sys_getpid() {
    return get_current()->pid; // 回傳目前任務的 PID
}

size_t sys_uart_read(char buf[], size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = uart_getc(); 
    }
    return size;
}

size_t sys_uart_write(const char buf[], size_t size) {
    for (size_t i = 0; i < size; i++) {
        uart_putc(buf[i]);
    }
    return size;
}

int sys_exec(const char *filename) {
    struct task_struct *curr = get_current();
    if (curr->user_stack) {
        free((void *)curr->user_stack); 
        curr->user_stack = 0; // 清空指標，防止 do_exec 出錯時造成 dangling pointer
    }
    // 重新載入程式碼
    // do_exec 內部會重新配置 user_stack 並設定 regs->epc
    int ret = do_exec(curr, filename);
    
    if (ret < 0) return -1;

    return 0; 
}

int sys_fork(struct pt_regs *regs) {
    struct task_struct *parent = get_current();
    
    // 建立子程序結構並清零
    struct task_struct *child = (struct task_struct *)allocate(sizeof(struct task_struct));
    memzero(child, sizeof(struct task_struct));
    
    // 分配並複製核心棧 (包含 Trap Frame)
    child->stack = (unsigned long)allocate(STACK_SIZE);
    memcpy((void*)(child->stack), (void*)(parent->stack), STACK_SIZE);

    // 基本資訊與訊號繼承
    uint64_t s = disable_and_save_sstatus();
    child->pid = nr_threads++;
    restore_sstatus(s);
    
    child->state = THREAD_RUNNING;
    
    // 複製訊號處理函式
    for(int i = 0; i < 32; i++) {
        child->sig_handlers[i] = parent->sig_handlers[i];
    }

    // 處理使用者棧
    child->user_stack = (unsigned long)allocate(STACK_SIZE);
    memcpy((void*)child->user_stack, (void*)parent->user_stack, STACK_SIZE);
    
    unsigned long sp_offset = regs->sp - parent->user_stack; 
    child->user_sp = child->user_stack + sp_offset;

    // 修正子程序的 Trap Frame (regs)
    // 計算 regs 在新 stack 中的位址
    struct pt_regs *child_regs = (struct pt_regs *)(child->stack + ((unsigned long)regs - parent->stack));
    child_regs->sp = child->user_sp;   
    child_regs->a0 = 0;                // 子程序回傳值為 0
    child_regs->tp = (unsigned long)child; 
    child_regs->epc += 4;              // 跳過父程序呼叫的 ecall

    // 設定上下文切換所需的資訊
    extern char ret_from_exception[];
    child->kernel_sp = child->stack + STACK_SIZE;
    
    // 繼承父程序的核心態 sstatus 設置
    child->thread.sstatus = parent->thread.sstatus; 
    child->thread.ra = (unsigned long)ret_from_exception;
    child->thread.sp = (unsigned long)child_regs;
    child->thread.sstatus = parent->thread.sstatus;
    enqueue(&run_queue, child);
    return child->pid; 
}

long sys_waitpid(int pid) {
    while (1) {
        int found = 0;
        int is_zombie = 0;

        // 遍歷所有任務來尋找該 PID
        uint64_t s = disable_and_save_sstatus();
        struct task_struct *p = run_queue;
        if (p) {
            do {
                if (p->pid == pid) {
                    found = 1;
                    if (p->state == THREAD_ZOMBIE) is_zombie = 1;
                    break;
                }
                p = p->next;
            } while (p != run_queue);
        }
        restore_sstatus(s);

        // 如果找不到該進程，回傳錯誤
        if (!found) return -1;
        
        // 如果已經是殭屍，代表結束了，回傳該 PID
        if (is_zombie) return pid;

        // 如果還在跑，父程序就主動讓出 CPU
        schedule(); 
    }
}

void sys_exit() {
    thread_exit(); // 呼叫 thread_exit 標記為 ZOMBIE 並排程
}

int sys_stop(int pid) {
    struct task_struct *curr = run_queue;
    if (!curr) return -1;

    do {
        if (curr->pid == pid) {
            // 不能殺掉正在跑的自己 
            if (curr == get_current()) return -1;

            // 從 run_queue 移除
            dequeue(curr); 
            
            // 標記為 ZOMBIE 並移入清理隊列
            curr->state = THREAD_ZOMBIE;
            curr->next = zombie_queue;
            zombie_queue = curr;
            
            return 0; // 成功停止
        }
        curr = curr->next;
    } while (curr != run_queue);

    return -1; // 找不到該 PID
}

int sys_usleep(unsigned int usec) {
    unsigned long start_time, current_time;
    
    // 讀取當前的硬體計數器值
    asm volatile("rdtime %0" : "=r"(start_time));

    // 計算目標週期數
    // 加上 (1000000 - 1) 為了向上取整，確保至少延遲足夠時間
    unsigned long diff = (unsigned long)usec * (cpu_freq / 1000000);

    while (1) {
        asm volatile("rdtime %0" : "=r"(current_time));
        if (current_time - start_time >= diff) {
            break;
        }
    }
    return 0; // Success
}

void sys_video_bmp_display(unsigned int* bmp_image, int width, int height) {
    video_bmp_display(bmp_image, width, height);
}

long sys_signal(int signum, void (*handler)()) {
    if (signum < 0 || signum >= 32) return -1;
    struct task_struct *curr = get_current();
    
    // 註冊 handler 位址
    curr->sig_handlers[signum] = handler;
    return 0;
}

int sys_kill(int pid, int signum) {
    if (signum < 0 || signum >= 32) return -1;
    
    struct task_struct *target = NULL;
    uint64_t s = disable_and_save_sstatus();
    struct task_struct *curr = run_queue;
    
    // 尋找目標行程
    if (curr) {
        do {
            if (curr->pid == pid) {
                target = curr;
                break;
            }
            curr = curr->next;
        } while (curr != run_queue);
    }
    restore_sstatus(s);

    if (!target) return -1;

    // 檢查是否有註冊 handler
    if (target->sig_handlers[signum]) {
        // 標記訊號為 Pending，等待返回 U-mode 前處理
        target->pending_signals |= (1 << signum);
    } else {
        // 直接終止
        sys_stop(pid);
        return -1;
    }
    return 0;
}

void sys_sigreturn(struct pt_regs *regs) {
    struct task_struct *curr = get_current();
    
    //uart_puts("Signal handler finished, returning to original context.\n");

    // 還原備份的 Trap Frame (從上次備份的 saved_regs 拷貝回來)
    if (curr->saved_regs) {
        memcpy(regs, curr->saved_regs, sizeof(struct pt_regs));
        
        // 回收臨時用的 Signal Stack
        free((void*)curr->signal_stack);
        free(curr->saved_regs);
        curr->saved_regs = NULL;
        curr->signal_stack = 0;
    }
}