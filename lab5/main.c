#include "sched.h"
#include "utils.h"
#include "memory.h"
#include "uart.h"
#include "fdt.h"
#include <stdint.h>
#include "trap.h"
#include "cpio.h"
#include "timer.h"
#include "plic.h"

uint64_t hart_id;

char input[128];
int input_idx = 0;

void shell_main_thread() {
    char c;
    int special_c = 0;
    
    uart_puts("opi-rv2> ");

    while (1) {
        // 核心邏輯：從核心的 Ring Buffer 拿字元
        // 如果 Buffer 沒資料，這個呼叫會觸發 schedule() 讓出 CPU
        c = uart_getc(); 

        if (c == '\r' || c == '\n') {
            uart_putc('\n');
            input[input_idx] = '\0';

            // 解析指令
            char* args[5] = {"", "", "", "", ""};
            int idx = 0;
            char* cmd = input;
            char* end = input;

            // 簡單的 strtok 模擬邏輯
            while (*end != '\0') {
                if (*end == ' ') {
                    *end = '\0';
                    end++;
                    while (*end == ' ') end++; // 跳過多餘空格
                    if (idx < 5) args[idx++] = end;
                } else {
                    end++;
                }
            }

            // 指令判斷
            if (strcmp(cmd, "exec") == 0) {
                struct task_struct* child = create_user_process(args[0]);
                if (child) {                    
                    // Kernel Shell 必須等待子程序結束, 這樣才不會跟子程序搶 UART 的字元
                    while (child->state != THREAD_ZOMBIE) {
                        schedule(); // 讓出 CPU 給子程序跑
                    }
                    
                } else {
                    uart_printf("[Kernel] Failed to execute %s\n", args[0]);
                }
            } 
            else if (strcmp(cmd, "help") == 0) {
                uart_puts("Available commands:\n");
                uart_puts("\thelp\t- show all commands.\n");
                uart_puts("\thello\t- print Hello World!\n");
                uart_puts("\texec\t- execute typed user-program.\n");
            } 
            else if (strcmp(cmd, "hello") == 0) {
                uart_puts("Hello World!\n");
            }
            else {
                if (strlen(cmd) != 0) uart_printf("%s: Command not found!\n", cmd);
            }

            // 重置狀態
            input_idx = 0;
            uart_puts("opi-rv2> ");

        } else if (c == 127 || c == '\b') {
            if (input_idx > 0) {
                uart_puts("\b \b");
                input_idx--;
            }
        } else if (c >= 32 && c <= 126) {
            // 處理 Escape Sequence (如方向鍵)
            if (special_c != 0) {
                special_c++;
                if (special_c == 3) special_c = 0;
                continue;
            }
            if (input_idx < 127) {
                uart_putc(c);
                input[input_idx++] = c;
            }
        } else if (c == 27) {
            special_c = 1;
        }
    }
}

int do_exec(struct task_struct *task, const char *filename) {
    // 載入程式碼
    void *code_start = cpio_get_file(filename); 
    if (!code_start) return -1;

    // 配置使用者棧頂端 (user_sp)
    task->user_stack = (unsigned long)allocate(STACK_SIZE); 
    task->user_sp = task->user_stack + STACK_SIZE; 

    // 在核心棧頂部預留 Trap Frame (pt_regs) 的空間
    struct pt_regs *regs = (struct pt_regs *)(task->kernel_sp - sizeof(struct pt_regs));
    // 初始化 Trap Frame
    memzero(regs, sizeof(struct pt_regs));
    regs->tp = (unsigned long)task;
    regs->epc = (unsigned long)code_start; // ecall 回傳後會從這跑
    regs->sp = task->user_sp;              // U-mode 的堆疊指標
    
    // 設定 sstatus：SPP=0 (降權到 U-mode)
    unsigned long sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus &= ~(1UL << 8); // 清除 SPP
    sstatus |= (1UL << 5);  // 開啟 SPIE
    regs->sstatus = sstatus;

    return 0;
}

void foo() {
    for (int i = 0; i < 5; i++) {
        uart_printf("Thread id: %d %d\n", get_current()->pid, i);
        for (volatile int d = 0; d < 100000000; d++);
        schedule();
    }
    
    thread_exit();
}

void start_kernel(uint64_t hartid, uint64_t fdt){
    hart_id = hartid;
    uart_puts("\n\n----------\n");
    
    memory_init(fdt);
    print_mm_buddy();

    cpio_init(fdt);
    uart_init();
    plic_init(hart_id, fdt);
    set_freq(fdt);
    init_timer();
    enable_timer_interrupt();
    irq_enable();
    extern void video_init();
    #ifdef DEBUG
        video_init();
    #endif

    uart_puts("\nStarting kernel ...\n\n");

    asm volatile("move tp, %0" : : "r"(thread_create(idle)));
    /*
    for (int i = 0; i < 3; i++) {
        thread_create(foo);
    }    
    idle();
    uart_puts("--- Exercise 1 Finished, Starting Shell ---\n\n");
    */
    thread_create(shell_main_thread);
    sbi_set_timer(get_ticks() + cpu_freq/32);
    idle();
    while(1);
}