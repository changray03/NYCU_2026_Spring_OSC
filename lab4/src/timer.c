#include <stdint.h>
#include "fdt.h"
#include "utils.h"
#include "timer.h"
#include "uart.h"

#define MAX_TIMERS 20  // 同時最多能設定的計時器數量
int boot_t = 0;
uint64_t target = 0;
uint64_t cpu_freq;

// 靜態池，避免在內核使用 malloc
static struct timer_event timer_pool[MAX_TIMERS];
// 始終指向「最快要到期」的計時器
static struct timer_event *timer_list_head = NULL;

struct timeout_info timeout_pool[10];
int timeout_idx = 0;

void my_timeout_callback(void *arg) {
    if(arg == NULL){
        uart_printf("\n[Timeout] NULL\n");    
    }
    else{
        char *msg = (char *)arg;
        uart_printf("\n[Timeout] %s\n", msg);
    }
}

void set_freq(uint64_t fdt){
    int offset = fdt_path_offset((const void*)fdt, "/cpus");
    int len;
    const uint32_t* prop = fdt_getprop((const void*)fdt, offset, "timebase-frequency", &len);
    cpu_freq = fdt32_to_cpu(prop);
    uart_printf("clock: %x\n", cpu_freq);
}

void enable_timer_interrupt(){
    asm volatile(
        "li t0, (1 << 5);"
        "csrs sie, t0;");
}
void disable_timer_interrupt(){
    asm volatile(
        "li t0, (1 << 5);"
        "csrc sie, t0;");    
}

void sbi_set_timer(uint64_t target_tick){
    asm volatile (
        "li a7, 0;"       
        "mv a0, %0;"      
        "ecall"           
        :
        : "r" (target_tick)
        : "a0", "a7"
    );
}

// 獲取當前硬體 Tick
uint64_t get_ticks() {
    uint64_t n;
    asm volatile("rdtime %0" : "=r"(n));
    return n;
}

// 初始化：清空池子
void init_timer(){
    for(int i = 0; i < MAX_TIMERS; i++) {
        timer_pool[i].is_used = 0;
    }
    timer_list_head = NULL;
}

void add_timer(void (*callback)(void*), void* arg, int sec) {
    uint64_t s = disable_and_save_sstatus(); // 保護臨界區

    // 從池中找一個空位
    struct timer_event *new_ev = NULL;
    for(int i = 0; i < MAX_TIMERS; i++) {
        if(!timer_pool[i].is_used) {
            new_ev = &timer_pool[i];
            break;
        }
    }

    if(!new_ev) {
        uart_printf("Error: No free timer slot!\n");
        restore_sstatus(s);
        return;
    }

    // 填充資料
    new_ev->timeout_tick = get_ticks() + (uint64_t)sec * cpu_freq;
    new_ev->callback = callback;
    new_ev->arg = arg;
    new_ev->is_used = 1;
    new_ev->next = NULL;

    // 插入排序鏈表 (由小到大)
    struct timer_event **p = &timer_list_head;
    while (*p != NULL && (*p)->timeout_tick < new_ev->timeout_tick) {
        p = &((*p)->next);
    }
    new_ev->next = *p;
    *p = new_ev;

    // 如果新加入的是「最快到期」的，立刻重新設定硬體鬧鐘
    if (timer_list_head == new_ev) {
        sbi_set_timer(new_ev->timeout_tick);
    }

    restore_sstatus(s);
}

void handle_timer_interrupt_task(void *arg) {
    uint64_t now = get_ticks();
    while (1) {
        // 進入臨界區：保護鏈表修改
        uint64_t s = disable_and_save_sstatus();
        
        if (timer_list_head == NULL || timer_list_head->timeout_tick > now) {
            restore_sstatus(s);
            break; 
        }

        struct timer_event *ev = timer_list_head;
        timer_list_head = ev->next; // 從鏈表移除
        restore_sstatus(s); // 離開臨界區

        // 執行 Callback (這部分可以開中斷跑)
        if (ev->callback) {
            ev->callback(ev->arg);
        }
        else{
            uart_puts("Nothing to execute!\n");
        }

        ev->is_used = 0;
        ev->next = NULL;
    }

    // 設定下一次中斷
    uint64_t s = disable_and_save_sstatus();
    if (timer_list_head != NULL) {
        sbi_set_timer(timer_list_head->timeout_tick);
    } else {
        sbi_set_timer(0xffffffffffffffff);
    }
    restore_sstatus(s);
    //enable_timer_interrupt();
}

void boot_time(void* arg){
    uart_printf("\nboot time: %d\n", boot_t);
    boot_t += 2;
    add_timer(boot_time, NULL, 2);
}