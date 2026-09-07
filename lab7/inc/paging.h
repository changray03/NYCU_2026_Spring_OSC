#ifndef PAGING_H
#define PAGING_H

#include <stddef.h>
#include "trap.h"

#define PAGE_OFFSET   0xffffffc000000000UL
#define PAGE_SIZE     (1UL << 12) 
#define PMD_SIZE      (1UL << 21)
#define PGD_SIZE      (1UL << 30)

/* VA bit-field shifts (Sv39) */
#define PGD_SHIFT     30 
#define PMD_SHIFT     21
#define PTE_SHIFT     12

#define ENTRIES_PER_TABLE  512

#ifdef DEBUG
    #define RAM_START 0x80000000UL
#else
    #define RAM_START 0x00000000UL
#endif
#define KERNEL_PGD_INDEX   ((PAGE_OFFSET >> PGD_SHIFT) & 0x1FF)

#ifdef DEBUG
    #define LINEAR_MAP_GIB     1
    #define NUM_PAGES          (0x40000000 >> 12)
#else
    #define LINEAR_MAP_GIB     2
    #define NUM_PAGES          (0x80000000 >> 12)
#endif

/* PTE descriptor bits (Sv39) */
#define PTE_V  (1UL << 0)  
#define PTE_R  (1UL << 1)
#define PTE_W  (1UL << 2)
#define PTE_X  (1UL << 3)
#define PTE_U  (1UL << 4)
#define PTE_G  (1UL << 5)
#define PTE_A  (1UL << 6)
#define PTE_D  (1UL << 7)
#define PTE_COW (1UL << 8)
#define PTE_SOFT (3UL << 8)

#define PROT_KERNEL    (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_A | PTE_D)
#define PROT_KERNEL_RW (PTE_V | PTE_R | PTE_W | PTE_G | PTE_A | PTE_D)
#define PROT_USER_BASE (PTE_V | PTE_U | PTE_A | PTE_D)
#define PROT_USER_RX   (PROT_USER_BASE | PTE_R | PTE_X)
#define PROT_USER_RW   (PROT_USER_BASE | PTE_R | PTE_W)

#define SATP_SV39           (8UL << 60)
#define MAKE_SATP(pgd_pa)   (SATP_SV39 | ((unsigned long)(pgd_pa) >> 12)) // satp: MODE(63 ~ 60) | ASID(59 ~ 44) | PPN(43 ~ 0)

#define MAKE_PTE(pa, flags) ((((unsigned long)(pa)) >> 12) << 10 | (flags))

#define virt_to_phys(x) ((unsigned long)(x) - PAGE_OFFSET)
#define phys_to_virt(x) ((unsigned long)(x) + PAGE_OFFSET)

enum PTE_FUNC{
    CHECK_COW, 
    GET_PA,
    GET_PROT,
};

extern unsigned long *active_user_pgd;
extern unsigned long kernel_pgd[];

void map_pages(unsigned long va,
               unsigned long size,
               unsigned long pa,
               unsigned long prot);
void free_user_page_table(unsigned long *pgd);
void page_fault_handler(struct pt_regs *regs, unsigned long scause);
unsigned long iterate_page_table(unsigned long *pgd_base, unsigned long va, int func);
void fork_copy_page_table_cow(unsigned long *dst_pgd, unsigned long *src_pgd);
void increment_page_ref(unsigned long pa);
int decrement_page_ref(unsigned long pa);
void set_page_ref_init(unsigned long pa);

#endif