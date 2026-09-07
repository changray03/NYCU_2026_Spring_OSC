#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include <stddef.h>
#include "trap.h"

enum syscall_num{
    SYS_GETPID = 0, 
    SYS_UART_READ = 1,  
    SYS_UART_WRITE = 2, 
    SYS_EXEC = 3,
    SYS_FORK = 4,
    SYS_WAITPID = 5,
    SYS_EXIT = 6,
    SYS_STOP = 7,
    SYS_DISPLAY = 8,
    SYS_USLEEP = 9,
    SYS_SIGNAL = 10,
    SYS_SIGRETURN = 11,
    SYS_KILL = 12,
    SYS_MMAP = 13,
};

void handle_syscall(struct pt_regs *regs);
uint64_t sys_getpid();
size_t sys_uart_read(char buf[], size_t size);
size_t sys_uart_write(const char buf[], size_t size);
int sys_exec(const char *filename);
int sys_fork(struct pt_regs *regs);
long sys_waitpid(int pid);
void sys_exit();
int sys_stop(int pid);
void sys_video_bmp_display(unsigned int* bmp_image, int width, int height);
int sys_usleep(unsigned int usec);
long sys_signal(int signum, void (*handler)());
void sys_sigreturn(struct pt_regs *regs);
int sys_kill(int pid, int signum);
void *sys_mmap(void *addr, unsigned long length, int prot, int flags);

#endif