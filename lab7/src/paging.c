#include <stdint.h>
#include <stddef.h>
#include "paging.h"
#include "memory.h"
#include "utils.h"
#include "uart.h"
#include "plic.h"
#include "sched.h"
#include "trap.h"

unsigned long __attribute__((section(".data"), aligned(PAGE_SIZE)))
    kernel_pgd[ENTRIES_PER_TABLE] = { 0 };

static unsigned long __attribute__((section(".data"), aligned(PAGE_SIZE)))
    kernel_pmd[LINEAR_MAP_GIB][ENTRIES_PER_TABLE] = { { 0 } };

static unsigned long __attribute__((section(".data"), aligned(PAGE_SIZE)))
    mmio_pmd[ENTRIES_PER_TABLE] = { 0 };  // 管轄 vpn2 = 256 的 PMD 表

static unsigned long __attribute__((section(".data"), aligned(PAGE_SIZE)))
    uart_pte[ENTRIES_PER_TABLE] = { 0 };  // 管轄 UART 的 Level 0 PTE 表

static unsigned long __attribute__((section(".data"), aligned(PAGE_SIZE)))
    plic_pte_low[ENTRIES_PER_TABLE] = { 0 };  // 管轄 0x0C000000 ~ 0x0C1FFFFF (含 Enable 暫存器)

static unsigned long __attribute__((section(".data"), aligned(PAGE_SIZE)))
    plic_pte_high[ENTRIES_PER_TABLE] = { 0 }; // 管轄 0x0C200000 ~ 0x0C3FFFFF (含 0x201000 閾值暫存器)

// 記錄目前正在施工中的 User PGD 虛擬位址
unsigned long *active_user_pgd = NULL;
unsigned char page_ref_counts[NUM_PAGES] = {0};

// 增加某一頁實體肉體的引用計數
void increment_page_ref(unsigned long pa) {
    unsigned long page_idx = (pa - RAM_START) >> 12;
    if (page_idx < NUM_PAGES) {
        page_ref_counts[page_idx]++;
    }
}

// 減少某一頁實體肉體的引用計數，並回傳扣減後的最新計數
int decrement_page_ref(unsigned long pa) {
    unsigned long page_idx = (pa - RAM_START) >> 12;
    if (page_idx < NUM_PAGES) {
        if (page_ref_counts[page_idx] > 0) {
            page_ref_counts[page_idx]--;
        }
        return page_ref_counts[page_idx];
    }
    return 0;
}

// 初始化新分配分頁的計數為 1
void set_page_ref_init(unsigned long pa) {
    unsigned long page_idx = (pa - RAM_START) >> 12;
    if (page_idx < NUM_PAGES) {
        page_ref_counts[page_idx] = 1;
    }
}

void setup_vm(void)
{
    /* 填寫 MMIO 的 PGD, PMD 和 PTE */
    uint64_t fw_cfg_base = 0xffffffc010100000;

    #ifdef DEBUG
        kernel_pgd[KERNEL_PGD_INDEX] = MAKE_PTE(mmio_pmd, PTE_V);
    #else 
        kernel_pgd[KERNEL_PGD_INDEX + 3] = MAKE_PTE(mmio_pmd, PTE_V);
    #endif
    mmio_pmd[(PLIC_BASE >> 21) & 0x1FF]  = MAKE_PTE(plic_pte_low, PTE_V);   // 映射 0x0C000000 起始區
    mmio_pmd[((PLIC_BASE + PMD_SIZE) >> 21) & 0x1FF]  = MAKE_PTE(plic_pte_high, PTE_V);  // 映射 0x0C200000 起始區
    mmio_pmd[(UART_BASE >> 21) & 0x1FF] = MAKE_PTE(uart_pte, PTE_V);       // 映射 UART 

    // 填寫最底層 PTE
    uart_pte[(UART_BASE >> 12) & 0x1FF] = MAKE_PTE(virt_to_phys(UART_BASE), PROT_KERNEL_RW);
    #ifdef DEBUG
        uart_pte[(fw_cfg_base >> 12) & 0x1FF] = MAKE_PTE(virt_to_phys(fw_cfg_base), PROT_KERNEL_RW); // fw_cfg mapping
    #endif

    // 填寫低位 PLIC PTE 
    for (int k = 0; k < ENTRIES_PER_TABLE; k++) {
        unsigned long plic_pa = virt_to_phys(PLIC_BASE) + k * PAGE_SIZE;
        plic_pte_low[k] = MAKE_PTE(plic_pa, PROT_KERNEL_RW);
    }

    // 填寫高位 PLIC PTE 
    for (int k = 0; k < ENTRIES_PER_TABLE; k++) {
        unsigned long plic_pa = virt_to_phys(PLIC_BASE + PMD_SIZE) + k * PAGE_SIZE;
        plic_pte_high[k] = MAKE_PTE(plic_pa, PROT_KERNEL_RW);
    }

    /* 填寫 RAM 的 PGD 和 PMD */
    for(int i = 0; i < LINEAR_MAP_GIB; i++){
        unsigned long pa = RAM_START + i * PGD_SIZE;
        int identity_idx = pa >> PGD_SHIFT;
        int kernel_idx = KERNEL_PGD_INDEX + identity_idx;
        kernel_pgd[identity_idx] = MAKE_PTE(kernel_pmd[i], PTE_V); // Identity
        kernel_pgd[kernel_idx] = MAKE_PTE(kernel_pmd[i], PTE_V); // High Map
        
        for(int j = 0; j < ENTRIES_PER_TABLE; j++){
            unsigned long pa_m = pa + j * PMD_SIZE;
            kernel_pmd[i][j] = MAKE_PTE(pa_m, PROT_KERNEL);
        }
    }

    // 開啟 MMU
    unsigned long satp_val = MAKE_SATP(kernel_pgd);
    asm volatile ("csrw satp, %0" : : "r"(satp_val));
    asm volatile ("sfence.vma" : : : "memory");
}

void drop_identity_map(void)
{   
    for (int i = 0; i < LINEAR_MAP_GIB; i++) {
        unsigned long pa = RAM_START + i * PGD_SIZE;
        int identity_idx = pa >> PGD_SHIFT;
        kernel_pgd[identity_idx] = 0;
    }

    // 再次刷新 TLB 確保舊的映射失效
    asm volatile ("sfence.vma" : : : "memory");
}

static void pagewalk(unsigned long va, unsigned long pa, unsigned long prot) {
    unsigned long vpn2 = (va >> 30) & 0x1FF;
    unsigned long vpn1 = (va >> 21) & 0x1FF;
    unsigned long vpn0 = (va >> 12) & 0x1FF;

    unsigned long *pgd_base = (va >= PAGE_OFFSET) ? kernel_pgd : active_user_pgd;

    if (!(pgd_base[vpn2] & PTE_V)) {
        unsigned long new_pmd_va = (unsigned long)allocate(PAGE_SIZE);

        memzero((void *)new_pmd_va, PAGE_SIZE);
        pgd_base[vpn2] = MAKE_PTE(virt_to_phys(new_pmd_va), PTE_V);
    }
    unsigned long pmd_pa = (pgd_base[vpn2] >> 10) << 12;
    unsigned long *pmd = (unsigned long *)phys_to_virt(pmd_pa);
    

    if (!(pmd[vpn1] & PTE_V)) {
        unsigned long new_pte_va = (unsigned long)allocate(PAGE_SIZE);

        memzero((void *)new_pte_va, PAGE_SIZE);
        pmd[vpn1] = MAKE_PTE(virt_to_phys(new_pte_va), PTE_V);
    }
    unsigned long pte_pa = (pmd[vpn1] >> 10) << 12;
    unsigned long *pte = (unsigned long *)phys_to_virt(pte_pa);

    pte[vpn0] = MAKE_PTE(pa, prot | PTE_V);
}

void map_pages(unsigned long va,
               unsigned long size,
               unsigned long pa,
               unsigned long prot) {
    for (uint64_t i = 0; i < size; i += PAGE_SIZE)
        pagewalk(va + i, pa + i, prot);
    asm volatile ("sfence.vma" : : : "memory");
}

void free_user_page_table(unsigned long *pgd) {
    if (!pgd) return;

    for (int i = 0; i < 256; i++) {
        if (pgd[i] & PTE_V) { 
            unsigned long pmd_pa = (pgd[i] >> 10) << 12;
            unsigned long *pmd = (unsigned long *)phys_to_virt(pmd_pa);

            for (int j = 0; j < 512; j++) {
                if (pmd[j] & PTE_V) { 
                    unsigned long pte_pa = (pmd[j] >> 10) << 12;
                    unsigned long *pte = (unsigned long *)phys_to_virt(pte_pa);
                    
                    // 巡邏最底層 Level 0 PTE
                    for (int k = 0; k < 512; k++) {
                        if (pte[k] & PTE_V) {
                            unsigned long page_pa = (pte[k] >> 10) << 12;

                            // 引用計數無差別執法：解除地圖映射，該實體頁引用數就 -1
                            int remaining_ref = decrement_page_ref(page_pa);

                            // 只有當全天下都沒有任何進程指著這塊肉體時，才真正呼叫 free 釋放
                            if (remaining_ref == 0) {
                                free((void*)phys_to_virt(page_pa));
                            }

                            pte[k] = 0; // 清除門牌項目
                        }
                    }
                    free(pte); // 釋放 PTE 骨架
                }
            }
            free(pmd); // 釋放 PMD 骨架
        }
    }
    free(pgd); // 釋放 PGD 本體
}

// exec 清空 page table
void clear_user_space_mappings(unsigned long *pgd) {
    if (!pgd) return;

    for (int i = 0; i < 256; i++) {
        if (pgd[i] & PTE_V) { 
            unsigned long pmd_pa = (pgd[i] >> 10) << 12;
            unsigned long *pmd = (unsigned long *)phys_to_virt(pmd_pa);

            for (int j = 0; j < 512; j++) {
                if (pmd[j] & PTE_V) { 
                    unsigned long pte_pa = (pmd[j] >> 10) << 12;
                    unsigned long *pte = (unsigned long *)phys_to_virt(pte_pa);
                    
                    for (int k = 0; k < 512; k++) {
                        if (pte[k] & PTE_V) {
                            unsigned long page_pa = (pte[k] >> 10) << 12;

                            // 解除映射，將 fork 剛加的計數扣減回來！
                            int remaining_ref = decrement_page_ref(page_pa);

                            // ref count 為 0 代表可回收
                            if (remaining_ref == 0) {
                                free((void*)phys_to_virt(page_pa));
                            }
                            pte[k] = 0; 
                        }
                    }
                    free(pte); // 釋放這一頁 Level 0 PTE 骨架
                }
            }
            free(pmd); // 釋放這一頁 Level 1 PMD 骨架
            pgd[i] = 0; // 將頂層門牌抹除歸零，徹底回歸純真空狀態！
        }
    }
    asm volatile("sfence.vma" : : : "memory");
}

unsigned long iterate_page_table(unsigned long *pgd_base, unsigned long va, int func){
    unsigned long vpn2 = (va >> 30) & 0x1FF;
    unsigned long vpn1 = (va >> 21) & 0x1FF;
    unsigned long vpn0 = (va >> 12) & 0x1FF;

    if (!(pgd_base[vpn2] & PTE_V)) return 0;
    unsigned long pmd_pa = (pgd_base[vpn2] >> 10) << 12;
    unsigned long *pmd = (unsigned long *)phys_to_virt(pmd_pa);

    if (!(pmd[vpn1] & PTE_V)) return 0;
    unsigned long pte_pa = (pmd[vpn1] >> 10) << 12;
    unsigned long *pte = (unsigned long *)phys_to_virt(pte_pa);

    if (!(pte[vpn0] & PTE_V)) return 0;

    switch(func){
        case CHECK_COW:
            return (pte[vpn0] & PTE_COW) ? 1 : 0;
            break;
        case GET_PA:
            return (pte[vpn0] >> 10) << 12; 
            break;
        case GET_PROT:
            return (pte[vpn0] & 0x3FF) & ~PTE_COW;
            break;
        default:
            uart_printf("Unknown iteration!!\n");
            while(1);
            break; 
    }
    return 0;
}

// CoW 走訪並複製父程序的頁表樹，並強行將權限設為 read-only
void fork_copy_page_table_cow(unsigned long *dst_pgd, unsigned long *src_pgd) {
    for (int i = 0; i < 256; i++) { 
        if (src_pgd[i] & PTE_V) {
            
            // 如果子程序的 Level 2 PGD 對應位置還是空的，幫他蓋一個 Level 1 PMD 骨架
            if (!(dst_pgd[i] & PTE_V)) {
                void *new_pmd = allocate(PAGE_SIZE);
                memzero(new_pmd, PAGE_SIZE);
                dst_pgd[i] = MAKE_PTE(virt_to_phys((unsigned long)new_pmd), PTE_V);
            }
            unsigned long src_pmd_pa = (src_pgd[i] >> 10) << 12;
            unsigned long *src_pmd = (unsigned long *)phys_to_virt(src_pmd_pa);
            unsigned long dst_pmd_pa = (dst_pgd[i] >> 10) << 12;
            unsigned long *dst_pmd = (unsigned long *)phys_to_virt(dst_pmd_pa);

            for (int j = 0; j < 512; j++) {
                if (src_pmd[j] & PTE_V) {
                    
                    // 如果子程序的 Level 1 PMD 對應位置是空的，幫他蓋一個 Level 0 PTE 骨架
                    if (!(dst_pmd[j] & PTE_V)) {
                        void *new_pte = allocate(PAGE_SIZE);
                        memzero(new_pte, PAGE_SIZE);
                        dst_pmd[j] = MAKE_PTE(virt_to_phys((unsigned long)new_pte), PTE_V);
                    }
                    unsigned long src_pte_pa = (src_pmd[j] >> 10) << 12;
                    unsigned long *src_pte = (unsigned long *)phys_to_virt(src_pte_pa);
                    unsigned long dst_pte_pa = (dst_pmd[j] >> 10) << 12;
                    unsigned long *dst_pte = (unsigned long *)phys_to_virt(dst_pte_pa);

                    for (int k = 0; k < 512; k++) {
                        if (src_pte[k] & PTE_V) {
                            unsigned long pte_val = src_pte[k];
                            
                            // 如果發現這個分頁本來是「可寫 (PTE_W)」的
                            if (pte_val & PTE_W) {
                                pte_val &= ~PTE_W;   // 剝奪可寫權限，使硬體 MMU 進入唯讀防禦
                                pte_val |= PTE_COW;  // 烙上軟體 CoW 印章，通知救火隊這是合法共用頁
                                src_pte[k] = pte_val; // 父程序自己也必須被改成唯讀！
                            }
                            increment_page_ref((pte_val >> 10) << 12);
                            // 將這份被降級、指引向同一個實體 PA 的地圖，同步複製給子程序
                            dst_pte[k] = pte_val;
                        }
                    }
                }
            }
        }
    }
    // 因為我們修改了當前活著的父程序頁表屬性，必須立刻沖刷硬體 TLB 快取！
    asm volatile("sfence.vma" : : : "memory");
}

void page_fault_handler(struct pt_regs *regs, unsigned long scause) {
    struct task_struct *curr = get_current();
    unsigned long fault_addr;
    
    // 從 stval 中，讀出是哪一個 va 造成 page fault
    asm volatile("csrr %0, stval" : "=r"(fault_addr));

    // 向下對齊 page size
    unsigned long page_va = fault_addr & ~4095UL;

    // CoW 之 store page fault
    if (scause == 15 && iterate_page_table(curr->pgd, fault_addr, CHECK_COW)) {

        uart_printf("[Permission fault]: %x\n", fault_addr);
        // 真的有人動手寫了，這時才向夥伴系統要一頁「新鮮實體肉體」
        void *new_pa_page = allocate(PAGE_SIZE);
        memzero(new_pa_page, PAGE_SIZE);
        set_page_ref_init(virt_to_phys(new_pa_page));
        
        // 把共用的那一頁舊內容，複製 4KB 到新頁面中 
        unsigned long old_pa = iterate_page_table(curr->pgd, fault_addr, GET_PA);
        memcpy(new_pa_page, (void*)phys_to_virt(old_pa), PAGE_SIZE);
        asm volatile("fence.i");
        
        // 把被剝奪的權限還給他：PTE_W = 1，並把軟體標記 PTE_COW 設為 0
        unsigned long new_pa = virt_to_phys((unsigned long)new_pa_page);
        unsigned long updated_prot = iterate_page_table(curr->pgd, fault_addr, GET_PROT) | PTE_W; // 還原可寫
        
        active_user_pgd = curr->pgd;
        // map 到新的 pa
        map_pages(page_va, PAGE_SIZE, new_pa, updated_prot);
        asm volatile("sfence.vma" : : : "memory");

        int remaining = decrement_page_ref(old_pa); // 宣告目前進程脫離共享，計數 -1
        if (remaining == 0) {
            free((void*)phys_to_virt(old_pa));      // 如果此頁面 reference 為 0, 直接 free 回收
        }
        return; 
    }

    // 檢查是不是落在合法的 Mmap 註冊區間
    for (int i = 0; i < curr->vma_count; i++) {
        if (fault_addr >= curr->vmas[i].start && fault_addr < curr->vmas[i].end) {
            
            // 檢查當前的車禍原因 (scause)，是否符合當初申請的 prot 權限
            int vma_prot = curr->vmas[i].prot;
            
            if (scause == 15 && !(vma_prot & 2)) { 
                // 狀況：Store Page Fault (15) 發生，但沒有 PROT_WRITE (2)權限
                break; 
            }
            if (scause == 13 && !(vma_prot & 1)) { 
                // 狀況：Load Page Fault (13) 發生，但該區沒有 PROT_READ (1) 權限
                break; 
            }
            if (scause == 12 && !(vma_prot & 4)) { 
                // 狀況：Instruction Page Fault (12) 發生，但該區沒有 PROT_EXEC (4) 權限
                break; 
            }

            // 權限審查通過，代表是合法的 Demand Paging
            uart_printf("[Mmap Translation fault]: %x\n", fault_addr);

            // 動態配置 4KB 實體肉體
            void *pa_page = allocate(PAGE_SIZE);
            memzero(pa_page, PAGE_SIZE);
            unsigned long pa = virt_to_phys((unsigned long)pa_page);
            set_page_ref_init(pa);

            // 解析該 VMA 當初註冊的權限
            int prot = curr->vmas[i].prot;
            unsigned long page_prot = (1UL << 0) | (1UL << 4) | (1UL << 6) | (1UL << 7); 
            if (prot & 1) page_prot |= (1UL << 1);
            if (prot & 2) { page_prot |= (1UL << 2); page_prot |= (1UL << 1); }
            if (prot & 4) page_prot |= (1UL << 3);

            // 精準對接這一頁，並洗淨 TLB
            extern unsigned long *active_user_pgd;
            active_user_pgd = curr->pgd;
            map_pages(page_va, PAGE_SIZE, pa, page_prot);
            asm volatile("sfence.vma" : : : "memory");
            return; // 救火成功，原路返回
        }
    }

    // 檢查是不是落在合法的 User Stack 區間 (0x3ffffff000)
    if (fault_addr >= 0x3fffffc000UL && fault_addr < 0x3fffffc000UL + STACK_SIZE) {
        uart_printf("[Stack Translation fault]: %x\n", fault_addr);

        // 計算目前踩到的這一頁，對應到當初 allocate 的 user_stack 的哪個物理偏移量
        void *pa_page = allocate(PAGE_SIZE);
        if (!pa_page) {
            uart_printf("[Stack Error]: Out of memory!\n");
            thread_exit();
        }
        memzero(pa_page, PAGE_SIZE);
        unsigned long pa = virt_to_phys((unsigned long)pa_page);
        set_page_ref_init(pa);

        // 織網通車 (Stack 固定為可讀可寫 PROT_USER_RW)
        active_user_pgd = curr->pgd;
        map_pages(page_va, PAGE_SIZE, pa, PROT_USER_RW);
        asm volatile("sfence.vma" : : : "memory");
        return;
    }

    // 檢查是不是落在合法的 User Code 區間
    if (fault_addr >= 0x0 && fault_addr < curr->code_size) {
        uart_printf("[Code Translation fault]: %x\n", fault_addr);

        void *pa_page = allocate(PAGE_SIZE);
        if (!pa_page) {
            uart_printf("[Code Error]: Out of memory!\n");
            thread_exit();
        }
        memzero(pa_page, PAGE_SIZE);
        set_page_ref_init(virt_to_phys(pa_page));

        unsigned long count = curr->code_size - page_va;
        if (count > PAGE_SIZE) {
            count = PAGE_SIZE;
        }

        // 精準算出這一頁對應到 CPIO 檔案內部的虛擬位址起點，並拷貝 4KB 過去
        void *src_va = (void *)(phys_to_virt(curr->code_pa) + page_va);
        memcpy(pa_page, src_va, count);
        asm volatile("fence.i");

        // 織網通車 (Code 固定為可讀可執行 PROT_USER_RX)
        unsigned long pa = virt_to_phys((unsigned long)pa_page);
        active_user_pgd = curr->pgd;
        map_pages(page_va, PAGE_SIZE, pa, PROT_USER_RX);
        asm volatile("sfence.vma" : : : "memory");
        return;
    }

    uart_printf("[Segmentation fault]: Kill Process\n");
    thread_exit();
}