#include "syscall.h"
#include <stdint.h>
#include <stddef.h>
#include "trap.h"
#include "uart.h"
#include "sched.h"
#include "utils.h"
#include "memory.h"
#include "timer.h"
#include "paging.h"
#include "vfs.h"

#define MAP_ANONYMOUS 0x20
#define MAP_POPULATE  0x8000

static inline void enable_sum() {
    asm volatile("csrs sstatus, %0" : : "r"(1UL << 18));
}

static inline void disable_sum() {
    asm volatile("csrc sstatus, %0" : : "r"(1UL << 18));
}

extern int do_exec(struct task_struct *task, const char *filename);
extern void video_bmp_display(unsigned int* bmp_image, int width, int height);

void handle_syscall(struct pt_regs *regs) {
    int syscall_num = regs->a7;
    int skip_epc_add = 0;
    enable_sum();
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

        case SYS_MMAP:
            regs->a0 = (unsigned long)sys_mmap((void*)regs->a0, (unsigned long)regs->a1, regs->a2, regs->a3);
            break;
        
        case SYS_OPEN:
            regs->a0 = sys_open((const char*)regs->a0, (int)regs->a1);
            break;

        case SYS_CLOSE:
            regs->a0 = sys_close((int)regs->a0);
            break;

        case SYS_READ:
            regs->a0 = sys_read((int)regs->a0, (void*)regs->a1, (size_t)regs->a2);
            break;

        case SYS_WRITE:
            regs->a0 = sys_write((int)regs->a0, (const void*)regs->a1, (size_t)regs->a2);
            break;

        case SYS_MKDIR:
            regs->a0 = sys_mkdir((const char*)regs->a0, regs->a1);
            break;

        case SYS_MOUNT:
            regs->a0 = sys_mount((const char*)regs->a0, (const char*)regs->a1, (const char*)regs->a2, regs->a3, (const void*)regs->a4);
            break;

        case SYS_CHDIR:
            regs->a0 = sys_chdir((const char*)regs->a0);
            break;

        case SYS_LSEEK64:
            regs->a0 = sys_lseek64((int)regs->a0, (long)regs->a1, (int)regs->a2);
            break;

        case SYS_IOCTL:
            regs->a0 = sys_ioctl((int)regs->a0, (unsigned long)regs->a1, (void*)regs->a2);
            break;

        default:
            uart_puts("Unknown syscall\n");
            break;
    }
    if (!skip_epc_add) {
        regs->epc += 4; // 正常跳過 ecall
    }
    disable_sum();
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

    int ret = do_exec(curr, filename);
    
    if (ret < 0) return -1;

    return 0; 
}

int sys_fork(struct pt_regs *regs) {
    struct task_struct *parent = get_current(); 
    
    // 建立子程序結構並清零
    struct task_struct *child = (struct task_struct *)allocate(sizeof(struct task_struct)); 
    if (!child) return -1;
    memzero(child, sizeof(struct task_struct)); 
    
    // 分配並複製核心棧 (包含 Trap Frame)
    child->stack = (unsigned long)allocate(STACK_SIZE); 
    memcpy((void*)(child->stack), (void*)(parent->stack), STACK_SIZE); 

    // 為子程序 allocate page table 並複製 kernel space 到 user pgd
    child->pgd = (unsigned long *)allocate(PAGE_SIZE);
    if (!child->pgd) { free((void*)child->stack); free(child); return -1; }
    memzero(child->pgd, PAGE_SIZE);
    
    for (int i = 256; i < 512; i++) {
        child->pgd[i] = kernel_pgd[i];
    }

    // 基本資訊與訊號繼承
    uint64_t s = disable_and_save_sstatus(); 
    child->pid = nr_threads++; 
    restore_sstatus(s); 
    child->state = THREAD_RUNNING; 
    
    for(int i = 0; i < 32; i++) child->sig_handlers[i] = parent->sig_handlers[i]; 

    // 繼承父程序的 Mmap VMA 記帳本資訊，確保子程序了解哪些保留區合法
    child->vma_count = parent->vma_count;
    for (int i = 0; i < parent->vma_count; i++) {
        child->vmas[i] = parent->vmas[i];
    }
    
    // 直接繼承父程序的 virtual sp, code_pa 和 code_size
    child->user_sp = regs->sp; 
    child->code_pa = parent->code_pa; 
    child->code_size = parent->code_size; 

    fork_copy_page_table_cow(child->pgd, parent->pgd);

    // 直接讓子進程的 cwd 指標指向與父進程相同的 vnode
    child->cwd = parent->cwd;

    // 將父進程打開的所有檔案 handle 淺拷貝至子進程對應的槽位
    for (int i = 0; i < MAX_FD; i++) {
        if (parent->files.fds[i] != NULL) {
            // 淺拷貝指針
            child->files.fds[i] = parent->files.fds[i];
            
            // 既然子進程也共享了它，將它的參考計數安全地遞增！
            child->files.fds[i]->ref_count++;
        } else {
            child->files.fds[i] = NULL;
        }
    }

    // 修正子程序的 Trap Frame (regs)
    struct pt_regs *child_regs = (struct pt_regs *)(child->stack + ((unsigned long)regs - parent->stack)); 
    child_regs->sp = child->user_sp;  
    child_regs->a0 = 0;                
    child_regs->tp = (unsigned long)child; 
    child_regs->epc += 4;              // 跳過父程序呼叫的 ecall

    // 設定上下文切換所需的資訊
    extern char ret_from_exception[]; 
    child->kernel_sp = child->stack + STACK_SIZE; 
    
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
    while(1);
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

static int check_vma_overlap(struct task_struct *curr, unsigned long start, unsigned long end) {
    // 檢查 mmap region 有無 overlap
    for (int i = 0; i < curr->vma_count; i++) {
        if (start < curr->vmas[i].end && end > curr->vmas[i].start) {
            return 1; 
        }
    }
    
    // 檢查 stack 有無 overlap
    if (start < 0x3fffffc000UL + STACK_SIZE && end > 0x3fffffc000UL) {
        return 1; 
    }

    // 檢查 code 有無 overlap
    if (start < curr->code_size) {
        return 1; 
    }

    return 0; 
}

static unsigned long find_free_vma_space(struct task_struct *curr, unsigned long length) {
    unsigned long base = 0x60000000UL; 
    // 確保自動選址的上限絕對不會壓到真實 Stack 的起點
    while (base + length <= 0x3fffffc000UL) { 
        if (!check_vma_overlap(curr, base, base + length)) {
            return base; 
        }
        base += 4096; 
    }
    return 0; 
}

void *sys_mmap(void *addr, unsigned long length, int prot, int flags) {
    struct task_struct *curr = get_current();
    
    if (curr->vma_count >= 32) return (void *)-1;

    // 無條件對齊到 4KB 邊界
    unsigned long aligned_len = align(length, PAGE_SIZE);
    if (aligned_len == 0) return (void *)-1;

    unsigned long va_start = (unsigned long)addr;

    // 檢查 addr 的合法性，不合法則找一個合法的 va
    if (va_start == 0 || (va_start & 4095) != 0 || check_vma_overlap(curr, va_start, va_start + aligned_len)) {
        va_start = find_free_vma_space(curr, aligned_len);
        if (va_start == 0) return (void *)-1;
    }

    curr->vmas[curr->vma_count].start = va_start;
    curr->vmas[curr->vma_count].end = va_start + aligned_len;
    curr->vmas[curr->vma_count].prot = prot;
    curr->vmas[curr->vma_count].flags = flags;
    curr->vma_count++;

    // 解析頁表權限位元
    unsigned long page_prot = PROT_USER_BASE; 
    if (prot & 1) page_prot |= PTE_R;
    if (prot & 2) { page_prot |= PTE_W; page_prot |= PTE_R; }
    if (prot & 4) page_prot |= PTE_X;

    // 只有 MAP_POPULATE 時才立刻動態分配肉體
    // 如果沒有，直接回傳 va_start 
    if (flags & MAP_POPULATE) {
        for (unsigned long offset = 0; offset < aligned_len; offset += PAGE_SIZE) {
            void *pa_page = allocate(PAGE_SIZE);
            if (!pa_page) return (void *)-1;
            memzero(pa_page, PAGE_SIZE);

            unsigned long pa = virt_to_phys((unsigned long)pa_page);
            set_page_ref_init(pa);
            active_user_pgd = curr->pgd;
            map_pages(va_start + offset, PAGE_SIZE, pa, page_prot);
        }
        asm volatile("sfence.vma" : : : "memory");
    }

    return (void *)va_start;
}

int sys_open(const char* pathname, int flags) {
    struct task_struct* curr = get_current();
    
    // 尋找空閒的 File Descriptor 槽位
    int fd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (curr->files.fds[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -1; // 超過最大開啟數量限制

    struct file* target_file = NULL;
    int ret = vfs_open(pathname, flags, &target_file);
    if (ret != 0) return ret; // 開啟或建立失敗，回傳負值錯誤碼

    // 填入進程的 FD 表
    curr->files.fds[fd] = target_file;
    return fd;
}

int sys_close(int fd) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    struct task_struct* curr = get_current();
    
    struct file* file = curr->files.fds[fd];
    if (!file) return -1;

    int ret = vfs_close(file);
    if (ret == 0) {
        curr->files.fds[fd] = NULL; // 清空槽位
    }
    return ret;
}

int sys_read(int fd, void* buf, size_t len) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    struct task_struct* curr = get_current();
    
    struct file* file = curr->files.fds[fd];
    if (!file) return -1;

    return vfs_read(file, buf, len);
}

int sys_write(int fd, const void* buf, size_t len) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    struct task_struct* curr = get_current();
    
    struct file* file = curr->files.fds[fd];
    if (!file) return -1;

    return vfs_write(file, buf, len);
}

int sys_mkdir(const char *pathname, unsigned mode) {
    // 告訴編譯器這個參數暫時不用，消除 Unused Warning
    (void)mode;
    return vfs_mkdir(pathname);
}

int sys_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data) {
    // 處理被忽略的參數
    (void)src;
    (void)flags;
    (void)data;
    return vfs_mount(target, filesystem);
}

int sys_chdir(const char* pathname) {
    struct task_struct* curr = get_current();
    return vfs_chdir(&(curr->cwd), pathname);
}

long sys_lseek64(int fd, long offset, int whence) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    struct task_struct* curr = get_current();
    struct file* file = curr->files.fds[fd];
    if (!file) return -1;

    return vfs_lseek64(file, offset, whence);
}

int sys_ioctl(int fd, unsigned long request, void* args) {
    if (fd < 0 || fd >= MAX_FD) return -1;
    struct task_struct* curr = get_current();
    struct file* file = curr->files.fds[fd];
    if (!file) return -1;

    return vfs_ioctl(file, request, args);
}