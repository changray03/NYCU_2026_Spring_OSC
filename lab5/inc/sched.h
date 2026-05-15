#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#define STACK_SIZE 0x1000

#define THREAD_RUNNING 0
#define THREAD_ZOMBIE  1
#define THREAD_WAITING 2

struct task_struct {
    struct thread_struct {
        unsigned long ra;
        unsigned long sp;
        unsigned long s[12];
        unsigned long sstatus;
    } thread;
    int pid;
    int state;
    unsigned long kernel_sp;
    unsigned long user_sp;
    unsigned long stack;
    unsigned long user_stack;
    void* sig_handlers[32];
    uint32_t pending_signals;
    struct pt_regs *saved_regs;
    unsigned long signal_stack;
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