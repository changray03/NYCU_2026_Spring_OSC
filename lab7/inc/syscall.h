#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include <stddef.h>
#include "trap.h"
#include "vfs.h"

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
    SYS_OPEN = 14,
    SYS_CLOSE = 15,
    SYS_READ = 16, 
    SYS_WRITE = 17,
    SYS_MKDIR = 18, 
    SYS_MOUNT = 19, 
    SYS_CHDIR = 20,
    SYS_LSEEK64 = 21,
    SYS_IOCTL = 22, 
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
int sys_open(const char* pathname, int flags);
int sys_close(int fd);
int sys_read(int fd, void* buf, size_t len);
int sys_write(int fd, const void* buf, size_t len);
int sys_mkdir(const char* pathname, unsigned mode);
int sys_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data);
int sys_chdir(const char* pathname);
long sys_lseek64(int fd, long offset, int whence);
int sys_ioctl(int fd, unsigned long request, void* args);

#endif