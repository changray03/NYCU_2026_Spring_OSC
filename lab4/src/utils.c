#include "utils.h"
#include "stdint.h"
#include "stddef.h"

extern char _end[];
static unsigned long curr = (unsigned long)&_end;

void* kmalloc(unsigned long size) {
    curr = (curr + 0xf) & ~0xf;
    void* p = (void*)curr;
    curr += size;
    return p;
}

void* alloc_page() {
    curr = (curr + 0x1000 - 1) & ~(0x1000 - 1);
    void* p = (void*)curr;
    curr += 0x1000;
    return p;
}

int hextoi(const char* s, int n) {
    int r = 0;
    while (n-- > 0) {
        r = r << 4;
        if (*s >= 'A')
            r += *s++ - 'A' + 10;
        else if (*s >= 0)
            r += *s++ - '0';
    }
    return r;
}

uint64_t align(uint64_t n, int byte) {
    return (n + byte - 1) & ~(byte - 1);
}

int memcmp(const void* s1, const void* s2, int n) {
    const unsigned char *a = s1, *b = s2;
    while (n-- > 0) {
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return 0;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; }
    return NULL;
}

inline const void *align4(const void *p) {
    return (const void *)(((uintptr_t)p + 3) & ~3);
}

/* 安全讀取大端序 32-bit (解決 Alignment Trap) */
uint32_t fdt32_to_cpu(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

void restore_sstatus(uint64_t sstatus) {
    if(sstatus & (1 << 1)) asm volatile("csrsi sstatus, (1 << 1)"); // sstatus.SIE
}

uint64_t disable_and_save_sstatus(){
    uint64_t sstatus;
    asm volatile("csrrci %0, sstatus, (1 << 1)" : "=r"(sstatus));
    return sstatus;
}

int atoi(char* s){
    int ret = 0;
    while(*s != '\0'){
        ret *= 10;
        ret += *s - '0';
        s++;
    }
    return ret;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    // 1. 複製字元，直到達到 n 個或是遇到來源字串結束
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // 2. 關鍵點：如果 src 結束了但還沒填滿 n 個字，剩下的全部補 '\0'
    for ( ; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

void irq_enable(){
    unsigned long mask = (1 << 1);
    asm volatile("csrs sstatus, %0" : : "r"(mask));
}
void irq_disable(){
    unsigned long mask = (1 << 1);
    asm volatile("csrc sstatus, %0" : : "r"(mask));
}
