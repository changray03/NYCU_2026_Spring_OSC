#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

void* kmalloc(unsigned long size);
void* alloc_page();
int hextoi(const char* s, int n);
uint64_t align(uint64_t n, int byte);
int memcmp(const void* s1, const void* s2, int n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
char *strchr(const char *s, int c);
uint32_t fdt32_to_cpu(const void *p);
void restore_sstatus(uint64_t sstatus);
uint64_t disable_and_save_sstatus();
int atoi(char* s);
char *strncpy(char *dest, const char *src, size_t n);
void irq_enable();
void irq_disable();

#endif