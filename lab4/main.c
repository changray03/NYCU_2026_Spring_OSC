#include "uart.h"
#include "utils.h"
#include "fdt.h"
#include "timer.h"
#include "cpio.h"
#include "plic.h"
#include "uart.h"
#include "task.h"

void* cpio_base;
uint64_t hart_id;
char input[128];
int input_idx = 0;
#define STACK_SIZE  0x1000

int exec(const char* filename) {
    current_task_priority = 999;
    char* p = (char*)cpio_base;
    if(!cpio_base) return 1;
    while (memcmp(p + sizeof(struct cpio_newc_header), "TRAILER!!!", 10)) {
        struct cpio_newc_header* hdr = (struct cpio_newc_header*)p;
        int namesize = hextoi(hdr->c_namesize, 8);
        int filesize = hextoi(hdr->c_filesize, 8);
        int headsize = align(sizeof(struct cpio_newc_header) + namesize, 4);
        int datasize = align(filesize, 4);
        if (!memcmp(p + sizeof(struct cpio_newc_header), filename, namesize)) {
            void* user_code = (void*)(p + headsize);

            // 配置用戶堆疊 (User Stack)
            void* user_stack = alloc_page();
            unsigned long user_sp = (unsigned long)user_stack + STACK_SIZE;

            // 設定 sstatus：將 SPP (bit 8) 清零以返回 U-mode, SPIE (bit 5) 為 1，確保返回後中斷功能正常
            unsigned long sstatus;
            asm volatile("csrr %0, sstatus" : "=r"(sstatus));
            sstatus &= ~(1UL << 8); // SPP = 0
            sstatus |= (1UL << 5);  // SPIE = 1
            asm volatile("csrw sstatus, %0" : : "r"(sstatus));

            // 設定 sepc 為程式進入點
            asm volatile("csrw sepc, %0" : : "r"(user_code));

            // 重要：將內核棧位址存入 sscratch
            // 這樣當 User Mode 發生 Trap 時，handle_exception 才能換回這個內核棧
            extern char _end[];
            asm volatile("csrw sscratch, %0" : : "r"(_end));

            // 切換堆疊並跳轉至 U-mode
            // 使用 sret 指令會自動根據 sepc 與 sstatus 返回
            asm volatile(
                "mv sp, %0\n"
                "sret\n"
                : : "r"(user_sp)
            );
            return 0;
        }
        p += headsize + datasize;
    }
    return 1;
}

// TODO: Define the trap frame structure
struct pt_regs {
    unsigned long ra;      // 8 * 0: Return address
    unsigned long sp;      // 8 * 1: 原本的 user sp (從 sscratch 讀取)
    unsigned long gp;      // 8 * 2: Global pointer
    unsigned long tp;      // 8 * 3: Thread pointer
    unsigned long t0;      // 8 * 4: Temporary 0
    unsigned long t1;      // 8 * 5: Temporary 1
    unsigned long t2;      // 8 * 6: Temporary 2
    unsigned long s0;      // 8 * 7: Saved register 0 / frame pointer
    unsigned long s1;      // 8 * 8: Saved register 1
    unsigned long a0;      // 8 * 9: Function argument 0 / return value
    unsigned long a1;      // 8 * 10: Function argument 1
    unsigned long a2;      // 8 * 11: ...
    unsigned long a3;      // 8 * 12:
    unsigned long a4;      // 8 * 13:
    unsigned long a5;      // 8 * 14:
    unsigned long a6;      // 8 * 15:
    unsigned long a7;      // 8 * 16:
    unsigned long s2;      // 8 * 17: Saved register 2
    unsigned long s3;      // 8 * 18: ...
    unsigned long s4;      // 8 * 19:
    unsigned long s5;      // 8 * 20:
    unsigned long s6;      // 8 * 21:
    unsigned long s7;      // 8 * 22:
    unsigned long s8;      // 8 * 23:
    unsigned long s9;      // 8 * 24:
    unsigned long s10;     // 8 * 25:
    unsigned long s11;     // 8 * 26:
    unsigned long t3;      // 8 * 27: Temporary 3
    unsigned long t4;      // 8 * 28: Temporary 4
    unsigned long t5;      // 8 * 29: Temporary 5
    unsigned long t6;      // 8 * 30: Temporary 6
    unsigned long epc;     // 8 * 31: Exception Program Counter
    unsigned long sstatus; // 8 * 32: Supervisor Status Register
    unsigned long scause;  // 8 * 33: Supervisor Cause Register
    unsigned long stval;   // 8 * 34: Supervisor Trap Value Register
};

void do_trap(struct pt_regs* regs) {
    int is_interrupt = (regs->scause & (1UL << 63)) ? 1 : 0;
    unsigned long cause_num = regs->scause & 0xFFF;

    if (is_interrupt) {
        switch (cause_num) {
            case 5: // Timer
                disable_timer_interrupt(); // Mask
                add_task(handle_timer_interrupt_task, NULL, 5);
                break;
            case 9: { // External
                int irq = plic_claim(hart_id);
                if (irq == UART_IRQ) uart_isr();
                if (irq) plic_complete(irq, hart_id);
                break;
            }
        }
    } else {
        // --- 處理異常 (Exception) ---
        switch (cause_num) {
            case 8: // Environment call from U-mode (ecall)
                irq_enable();
                uart_puts("=== S-Mode trap (ecall) ===\n");
                uart_printf("scause: %d\n", regs->scause);
                uart_printf("sepc: 0x%x\n", regs->epc);
                uart_printf("stval: %d\n", regs->stval);
                irq_disable();
                
                regs->epc += 4; 
                break;
            default:
                uart_printf("Exception occurred! cause: %d, epc: 0x%x\n", cause_num, regs->epc);
                while(1);
        }
    }
    run_preemptive_tasks();
}

void handle_shell_input(char c) {
    static int special_c = 0;
    if(c == '\r' || c == '\n'){
        uart_putc('\n');
        input[input_idx] = '\0';
        
        // 解析指令 
        char* args[5];
        int idx = 0;
        char* end = input;
        while(1){
            end = strchr(end, ' ');
            if(idx <= 4) args[idx] = (end) ? end+1 : "";
            if (end) *end = '\0';
            else break;
            do{ end++; }while(*end == ' ');
            idx++;
        }
        
        if(strcmp(input, "exec") == 0){
            if(exec(args[0])){
                uart_printf("%s does not exist\n", args[0]);
            }
        }
        else if(strcmp(input, "help") == 0){
            uart_puts("Available commands:\n");
            uart_puts("\thelp\t- show all commands.\n");
            uart_puts("\thello\t- print Hello World!\n");
            uart_puts("\texec\t- execute typed user-program.\n");
            uart_puts("\tsettimout\t- set up timer.\n");
        }
        else if(strcmp(input, "hello") == 0){
            uart_puts("Hello World!\n");
        }
        else if (strcmp(input, "settimeout") == 0) {
            int sec = atoi(args[0]);
            char *msg = args[1];

            struct timeout_info *info = &timeout_pool[timeout_idx];
            strncpy(info->message, msg, 64);
            timeout_idx = (timeout_idx + 1) % 10;

            uart_printf("sec: %d, msg: %s\n", sec, info->message);
            add_timer(my_timeout_callback, info->message, sec);
        }
        else{
            if(strlen(input) != 0) uart_printf("%s: Command not found!\n", input);
        }    
        
        input_idx = 0;
        uart_puts("opi-rv2> ");
    } else if(c == 127 || c == '\b') {
        if(input_idx > 0) {
            uart_puts("\b \b");
            input_idx--;
        }
    } else if (c >= 32 && c <= 126) {
        if(special_c != 0){
            special_c++;
            if(special_c == 3) special_c = 0;
            return;
        } 
        if (input_idx < 127) {
            uart_putc(c);
            input[input_idx++] = c;
        }
    }
    else if(c == 27){
        special_c = 1;
    }
}

void test_task_cb(void *arg) {
    uart_puts("[Task] Executing Priority ");
    if(arg == NULL){
        uart_puts("NULL\n");
        return;
    }
    uart_puts((char*)arg);
    uart_putc('\n');
    
}

int priority_set[4];

void p1_callback(){
    uart_puts("P1 start\n");
    uart_puts("P1 end\n");
}

void p3_callback(){
    uart_puts("P3 start\n");
    add_task(p1_callback, NULL, priority_set[0]);
    add_timer(NULL, NULL, 0);
    uart_puts("P3 end\n");
}

void p2_callback(){
    uart_puts("P2 start\n");
    add_task(p3_callback, NULL, priority_set[2]);
    add_timer(NULL, NULL, 0);
    uart_puts("P2 end\n");
}

void p4_callback(){
    uart_puts("P4 start\n");
    add_task(p2_callback, NULL, priority_set[1]);
    add_timer(NULL, NULL, 0);
    uart_puts("P4 end\n");
}

void test_func(){
    int from_small_to_big = 1; // set to 0 if the task with a smaller number has a higher priority
    if(from_small_to_big){
        priority_set[0] = 10;
        priority_set[1] = 20;
        priority_set[2] = 30;
        priority_set[3] = 40;
    }else{
        priority_set[0] = 40;
        priority_set[1] = 30;
        priority_set[2] = 20;
        priority_set[3] = 10;
    }

    add_task(p4_callback, NULL, priority_set[3]);
}

extern char handle_exception[];
extern char _start[];
extern char _end[];
void start_kernel(uint64_t hartid,  uint64_t fdt) {
    hart_id = hartid;
        // find cpio
    int chosen_off = fdt_path_offset((void *)fdt, "/chosen");
    if (chosen_off >= 0) {
        int len;
        const uint32_t *start_ptr = fdt_getprop((void *)fdt, chosen_off, "linux,initrd-start", &len);
        if (start_ptr) {
            if (len == 4) {
                cpio_base = (void *)(uintptr_t)fdt32_to_cpu(start_ptr);
            } else {
                cpio_base = (void *)(((uint64_t)fdt32_to_cpu(start_ptr) << 32) | fdt32_to_cpu(start_ptr + 1));
            }
        }
    }
    if (cpio_base) {
        uart_puts("Initrd found at: ");
        uart_hex((unsigned long)cpio_base);
        uart_puts("\n");
    } else {
        uart_puts("Error: Could not find initrd in devicetree.\n");
    }
    
    uart_init();
    plic_init(hart_id, fdt);
    set_freq(fdt);
    init_timer();
    enable_timer_interrupt();
    irq_enable();

    add_timer(boot_time, NULL, 2);
    uart_puts("\nStarting kernel ...\n\n");
    add_timer(test_func, NULL, 0);

    //run_preemptive_tasks();

    uart_puts("opi-rv2> ");
    while(1) {
        asm volatile("wfi"); // 主迴圈現在只需要睡覺，一切由中斷任務驅動
    }
}
