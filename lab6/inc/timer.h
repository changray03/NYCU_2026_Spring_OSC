#ifndef TIME_INTERRUPT_H
#define TIME_INTERRUPT_H

#include <stdint.h>

struct timeout_info {
    char message[64];
};
struct timer_event {
    uint64_t timeout_tick;     // 觸發的絕對時間 (ticks)
    void (*callback)(void*);   // 回呼函式
    void *arg;                 // 函式參數
    int is_used;               // 標記是否被使用
    struct timer_event *next;  // 鏈表指標
};
extern struct timeout_info timeout_pool[10];
extern int timeout_idx;
extern uint64_t cpu_freq;

void my_timeout_callback(void *arg);
void set_freq(uint64_t fdt);
void enable_timer_interrupt();
void disable_timer_interrupt();
void init_timer();
void sbi_set_timer(uint64_t target_tick);
uint64_t get_ticks();
void add_timer(void (*callback)(void*), void* arg, int sec);
void handle_timer_interrupt_task(void *arg);
void boot_time(void* arg);
void timer_handler();
#endif