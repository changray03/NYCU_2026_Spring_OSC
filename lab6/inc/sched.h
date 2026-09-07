#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#define STACK_SIZE 0x4000

#define THREAD_RUNNING 0
#define THREAD_ZOMBIE  1
#define THREAD_WAITING 2

struct vm_area {
    unsigned long start;   // 使用者虛擬空間起點 (Page-aligned)
    unsigned long end;     // 使用者虛擬空間終點 (Page-aligned)
    int prot;              // 原始的 PROT_READ / PROT_WRITE / PROT_EXEC
    int flags;             // MAP_ANONYMOUS 等旗標
};

struct task_struct {
    struct thread_struct {
        unsigned long ra;
        unsigned long sp;
        unsigned long s[12];
        unsigned long sstatus;
    } thread;
    int pid;
    int state;
    unsigned long *pgd;
    unsigned long code_pa;      // 🌟 新增：記錄當初 do_exec 分配給它的程式碼實體位址
    unsigned long code_size;

    unsigned long kernel_sp;
    unsigned long user_sp;
    unsigned long stack;
    unsigned long user_stack;

    void* sig_handlers[32];
    uint32_t pending_signals;
    struct pt_regs *saved_regs;
    unsigned long signal_stack;

    struct vm_area vmas[32];
    int vma_count;

    struct task_struct* next, *prev;
};

extern struct task_struct* run_queue;
extern struct task_struct* zombie_queue;
extern int nr_threads;

void enqueue(struct task_struct** queue, struct task_struct* task);
void dequeue(struct task_struct* task);
void schedule();
void thread_exit();
void kill_zombies();
struct task_struct* get_current();
struct task_struct* thread_create(void (*threadfn)());
void idle();
//int get_active_task_count();
struct task_struct* create_user_process(const char *filename);

#endif