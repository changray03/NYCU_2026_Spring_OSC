#include "sched.h"
#include "uart.h"
#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "trap.h"
#include "utils.h"

int nr_threads = 0;
struct task_struct* run_queue = NULL;
struct task_struct* zombie_queue = NULL;
extern int do_exec(struct task_struct *task, const char *filename);

void enqueue(struct task_struct** queue, struct task_struct* task) {
    uint64_t s = disable_and_save_sstatus();
    if (!*queue) {
        *queue = task;
        task->next = task;
        task->prev = task;
    } else {
        (*queue)->prev->next = task;
        task->next = *queue;
        task->prev = (*queue)->prev;
        (*queue)->prev = task;
    }
    restore_sstatus(s);
}

void dequeue(struct task_struct* task) {
    uint64_t s = disable_and_save_sstatus();
    if(task){
        task->next->prev = task->prev;
        task->prev->next = task->next;
        if(run_queue == task) run_queue = task->next;
        task->next = NULL;
        task->prev = NULL;
    }
    restore_sstatus(s);
}

void thread_exit() {
    uart_printf("Thread exiting!\n");
    uint64_t s = disable_and_save_sstatus();
    struct task_struct* curr = get_current();
    // 標記為 ZOMBIE 
    curr->state = THREAD_ZOMBIE;
    
    // 從 run_queue 中移除，確保不會再被排程 
    dequeue(curr);
    
    // 放入 zombie_queue 等待 idle 回收
    curr->next = zombie_queue;
    zombie_queue = curr;
    
    restore_sstatus(s);
    // 交出 CPU 控制權 
    schedule();
}

void kill_zombies() {
    if(zombie_queue) uart_printf("Cleaning zombie!\n");
    uint64_t s = disable_and_save_sstatus();
    struct task_struct* curr = zombie_queue;
    zombie_queue = NULL; // 先清空指標，防止重複回收
    
    while (curr) {
        struct task_struct* temp = curr;
        curr = curr->next;
        // 釋放資源 
        free((void*)temp->stack); 
        if(temp->user_stack) free((void*)temp->user_stack);
        free((void*)temp);
    }
    restore_sstatus(s);
}

struct task_struct* get_current() {
    register struct task_struct* current asm("tp");
    return current;
}

extern void switch_to(struct task_struct* prev, struct task_struct* next);

void schedule() {
    //uart_printf("%d\n", run_queue->pid);
    uint64_t s = disable_and_save_sstatus();
    if (get_current() == 0) uart_printf("Panic: tp is zero!\n");
    if(!run_queue){
        restore_sstatus(s);
        return;
    }
    struct task_struct* curr = get_current();
    if(curr == run_queue) run_queue = run_queue ->next;
    if (curr != run_queue) {
        // 更新 tp 暫存器，讓 get_current() 在切換後能拿到正確的 next
        //asm volatile("mv tp, %0" : : "r"(run_queue));
        // 行上下文切換
        switch_to(curr, run_queue);
    }
    restore_sstatus(s);
}

struct task_struct* thread_create(void (*threadfn)()) {
    // 配置 task_struct 空間
    struct task_struct* task = allocate(sizeof(struct task_struct));
    if(!task){
        return NULL;
    }
    memzero((void*)task, sizeof(struct task_struct));

    // task struct 初始化
    uint64_t s = disable_and_save_sstatus();
    task->pid = nr_threads++;
    restore_sstatus(s);
    task->state = THREAD_RUNNING;

    task->stack = (unsigned long)allocate((unsigned long)STACK_SIZE);
    if(!task->stack){
        free(task);
        return NULL;
    }
    task->user_stack = 0;

    // thread context member 初始化
    task->thread.ra = (unsigned long)threadfn;
    task->thread.sp = task->stack + STACK_SIZE;
    uint64_t sstatus;
    asm volatile("csrr %0, sstatus": "=r"(sstatus));
    task->thread.sstatus = sstatus;

    // signal member 初始化
    for(int i = 0; i< 32; i++) task->sig_handlers[i] = NULL;
    task->pending_signals = 0;
    task->saved_regs = NULL;
    task->signal_stack = 0;

    enqueue(&run_queue, task);
    return task;
}

void idle() {
    while(1){
        kill_zombies();
        schedule();
    }
}

struct task_struct* create_user_process(const char *filename) {
    // 配置 task_struct 空間
    struct task_struct *task = (struct task_struct *)allocate(sizeof(struct task_struct));
    if (!task){
        return NULL;
    } 
    memzero((void*)task, sizeof(struct task_struct));

    // task struct 初始化
    uint64_t s = disable_and_save_sstatus();
    task->pid = nr_threads++;
    restore_sstatus(s);
    task->state = THREAD_RUNNING;
    
    // 每個 thread 必須有自己獨立的核心棧，用來處理 Trap
    task->stack = (unsigned long)allocate(STACK_SIZE); 
    if (!task->stack) {
        free(task);
        return NULL;
    }
    task->user_sp = 0; // do_exec 會配置

    // signal member 初始化
    for(int i = 0; i< 32; i++) task->sig_handlers[i] = NULL;
    task->pending_signals = 0;
    task->saved_regs = NULL;
    task->signal_stack = 0;
    
    // 設定核心棧頂端，這就是 Trap 發生時 sscratch 要換進來的位址
    task->kernel_sp = task->stack + STACK_SIZE;
    
    // 呼叫核心版本 exec 載入程式碼並佈置 Trap Frame
    // 這會設定好 task->user_sp，並在核心棧頂部填好 epc, sstatus 等
    int ret = do_exec(task, filename);
    if (ret < 0) {
        free((void*)task->stack);
        free(task);
        return NULL;
    }
    
    // 醒來時，直接執行 ret_from_exception 彙編標籤
    extern char ret_from_exception[];
    task->thread.ra = (unsigned long)ret_from_exception;
    
    // 當 switch_to 換到這個 thread 並執行 ret 回到 ret_from_exception 時，
    // 彙編中的 ld 指令才能從正確的記憶體位置 pop 出暫存器內容
    task->thread.sp = task->kernel_sp - sizeof(struct pt_regs);
    uint64_t sstatus;
    asm volatile("csrr %0, sstatus": "=r"(sstatus));
    task->thread.sstatus = sstatus;
    // 放入排程隊列，等待下一次 schedule() 選中它
    enqueue(&run_queue, task);

    return task;
}

/*
int get_active_task_count() {
    int count = 0;
    uint64_t s = disable_and_save_sstatus();
    struct task_struct *p = run_queue; // 假設你維護一個所有 task 的鏈表
    while (1) {
        if (p->state != THREAD_ZOMBIE) {
            count++;
        }
        p = p->next;
        if(p == run_queue) break;
    }
    restore_sstatus(s);
    return count;
}
*/