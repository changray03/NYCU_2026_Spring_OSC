#include "memory.h"
#include "uart.h"
#include "fdt.h"

typedef unsigned long uint64_t;
typedef unsigned long size_t;
typedef unsigned int uint32_t;
typedef struct {
    uint64_t start;
    uint64_t end;
} reserved_region_t;

static reserved_region_t rg[64];
extern char _start[];
extern char _end[];

static uint32_t fdt32_to_cpu(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}
static uint64_t fdt64_to_cpu(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] << 8)  |  (uint64_t)b[7];
}
static inline const void *align4(const void *p) {
    return (const void *)(((uintptr_t)p + 3) & ~3);
}
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

void test_alloc_1() {
    /***************** Case 1 *****************/
    /*
    uart_puts("\n===== Part 1 =====\n");

    void *p1 = allocate(4097);
    free(p1);

    uart_puts("\n=== Part 1 End ===\n");

    uart_puts("\n===== Part 2 =====\n");

    // Allocate all blocks at order 0, 1, 2 and 3
    int NUM_BLOCKS_AT_ORDER_0 = 2;  // Need modified
    int NUM_BLOCKS_AT_ORDER_1 = 0;
    int NUM_BLOCKS_AT_ORDER_2 = 0;
    int NUM_BLOCKS_AT_ORDER_3 = 1;

    void *ps0[NUM_BLOCKS_AT_ORDER_0];
    void *ps1[NUM_BLOCKS_AT_ORDER_1];
    void *ps2[NUM_BLOCKS_AT_ORDER_2];
    void *ps3[NUM_BLOCKS_AT_ORDER_3];
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_0; ++i) {
        ps0[i] = allocate(4096);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_1; ++i) {
        ps1[i] = allocate(8192);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_2; ++i) {
        ps2[i] = allocate(16384);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_3; ++i) {
        ps3[i] = allocate(32768);
    }

    uart_puts("\n-----------\n");

    long MAX_BLOCK_SIZE = PAGE_SIZE * (1 << MAX_ORDER);
    */
    /* **DO NOT** uncomment this section */
    void *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;


    p2 = allocate(3769);
    p3 = allocate(2699);
    p4 = allocate(1028);
    p5 = allocate(1);
    p6 = allocate(4096);
    free(p5);                        // 1
    p7 = allocate(16000);
    free(p4);                        // 1028
    free(p2);                        // 3769
    p8 = allocate(4097);
    p9 = allocate(max_malloc_size + 1);
    p10 = allocate(max_malloc_size);
    free(p6);                        // 4096
    free(p8);                        // 4097
    p2 = allocate(7197);

    free(p10);                       // MAX_BLOCK_SIZE
    free(p7);                        // 16000
    free(p2);                        // 7197
    free(p3);                        // 2699

    uart_puts("\n-----------\n");
    /*
    // Free all blocks remaining
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_0; ++i) {
        free(ps0[i]);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_1; ++i) {
        free(ps1[i]);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_2; ++i) {
        free(ps2[i]);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_3; ++i) {
        free(ps3[i]);
    }
    */
    uart_puts("\n=== Part 2 End ===\n");
}

static void startup_allocator(uint64_t start, uint64_t size, uint64_t fdt) {
    int idx = 0;
    /* 找 DTB 的 start & end*/
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    const char *struct_base = (const char *)fdt + fdt32_to_cpu(&h->off_dt_struct);
    rg[idx].start = fdt;
    rg[idx].end = fdt + fdt32_to_cpu(&h->totalsize);
    uart_printf("DTB reserved region -> start: %x, end: %x\n", rg[idx].start, rg[idx].end);
    idx++;

    /* 找 Kernel Image 的 start & end */
    rg[idx].start = (uint64_t)_start;
    rg[idx].end = (uint64_t)_end;
    uart_printf("Kernel image reserved region -> start: %x, end: %x\n", rg[idx].start, rg[idx].end);
    idx++;

    /* 找 Initramfs */
    int offset = fdt_path_offset((const void *)fdt, "/chosen");
    
    if (offset >= 0) {
        int len;
        const uint32_t *ptr = fdt_getprop((void *)fdt, offset, "linux,initrd-start", &len);
        if (ptr) {
            if (len == 4) rg[idx].start = fdt32_to_cpu(ptr);
            else rg[idx].start = fdt64_to_cpu(ptr);
        }
        ptr = fdt_getprop((void *)fdt, offset, "linux,initrd-end", &len);
        if (ptr) {
            if (len == 4) rg[idx].end = fdt32_to_cpu(ptr);
            else rg[idx].end = fdt64_to_cpu(ptr);
        }
        uart_printf("Initramfs reserved region -> start: %x, end: %x\n", rg[idx].start, rg[idx].end);
        idx++;
    }
    else{
        uart_puts("Can't find initramfs!\n");
    }
    
    /* 找 /reserved-memory 中的 reg */
    uart_puts("/reserved-memory list:\n");
    offset = fdt_path_offset((const void *)fdt, "/reserved-memory");
    if (offset >= 0){
        // 越過 /reserved-memory 節點本身的 BEGIN_NODE 和 Name
        const uint32_t *p = (uint32_t *)(struct_base + offset);
        if(fdt32_to_cpu(p) != FDT_BEGIN_NODE) uart_puts("Something is weird!\n");
        p++;
        uart_printf("Current path: %s\n", (const char*)p);
        p = (const uint32_t *)align4((const char *)p + strlen((const char *)p) + 1);

        // 掃描並跳過父節點的所有屬性 (FDT_PROP)，直到遇見第一個子節點
        while(fdt32_to_cpu(p) != FDT_BEGIN_NODE){
            if(fdt32_to_cpu(p) == FDT_PROP){
                p++;
                uint32_t len = fdt32_to_cpu(p);
                p += 2; 
                p = (const uint32_t *)align4((const char *)p + len);
            }
            else{
                p++;
            }
        }
        
        // 此時 p 指向第一個子節點的 FDT_BEGIN_NODE 或父節點的 FDT_END_NODE
        while (fdt32_to_cpu(p) == FDT_BEGIN_NODE) {
            int child_offset = (char *)p - (char *)struct_base;
            p++;
            uart_printf("- %s ->", (const char*)p);
            p = (const uint32_t *)align4((const char*)p + strlen((const char *)p) + 1);
            
            int len = 0;
            const uint64_t* prop = (const uint64_t*)fdt_getprop((const void*)fdt, child_offset, "reg", &len);
            if(prop){ 
                rg[idx].start = fdt64_to_cpu(prop);
                rg[idx].end = rg[idx].start + fdt64_to_cpu(prop+1);
                uart_printf("start: %x, end: %x\n", rg[idx].start, rg[idx].end);
                idx++;
            }
            prop = (const uint64_t*)fdt_getprop((const void*)fdt, child_offset, "alloc-ranges", &len);
            if(prop){ 
                rg[idx].start = fdt64_to_cpu(prop);
                rg[idx].end = rg[idx].start + fdt64_to_cpu(prop+1);
                uart_printf("start: %x, end: %x\n", rg[idx].start, rg[idx].end);
                idx++;
            }

            p = skip_node(p);
            while(fdt32_to_cpu(p) == FDT_NOP) p++;
        }
    }

    /* 從 _end 後找放 struct frame 的 space */
    total_pages = size / PAGE_SIZE;
    max_malloc_size = total_pages * PAGE_SIZE;
    uint64_t metadata_start = ((uint64_t)_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t metadata_end = metadata_start + (total_pages * sizeof(struct frame));
    // 檢查是否與 reserved region 重疊
    int overlap = 1;
    while (overlap) {
        overlap = 0;
        metadata_end = metadata_start + (total_pages * sizeof(struct frame));
        
        for (int i = 0; i < idx; i++) {
            // 檢查 [metadata_start, metadata_end] 是否與 rg[i] 交集
            if (!(metadata_end <= rg[i].start || metadata_start >= rg[i].end)) {
                // 發生重疊，將起點移動到該重疊區域的結束位址，並重新對齊
                metadata_start = (rg[i].end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                overlap = 1;
                break; 
            }
        }
    }
    frames = (struct frame *)metadata_start;
    rg[idx].start = metadata_start;
    rg[idx].end = metadata_end;

    /* 初始化 buddy system */
    buddy_init(start);
    for (int i = 0; i <= idx; i++) {
        int start_idx = (rg[i].start - start) / PAGE_SIZE;
        int end_idx = (rg[i].end - start + PAGE_SIZE - 1) / PAGE_SIZE;
        for(int j = start_idx; j < end_idx; j++){
            if(j < total_pages) frames[j].status = FRAME_RESERVED;
        }
        uart_printf("index %d to %d is reserved!\n", start_idx, end_idx);
    }
    for (uint64_t i = 0; i < total_pages; i++) {
        // 看這一頁是否被標記為保留
        if (frames[i].status != FRAME_RESERVED) {
            uint64_t pa = start + (i * PAGE_SIZE);
            buddy_free((void*)pa, 0); // 讓它自動合併成大區塊 
        }
    }
    uart_puts("End of startup allocator!\n");
}

void start_kernel(uint64_t hardid, uint64_t fdt){
    uart_puts("\n\n----------\n");
    /* 找 /memory 下的 reg 屬性 */
    int offset = fdt_path_offset((const void *)fdt, "/memory");
    int len = 0;
    const uint64_t* prop = (const uint64_t*)fdt_getprop((const void *)fdt, offset, "reg", &len);
    uint64_t mm_start = fdt64_to_cpu(prop);
    uint64_t mm_size = fdt64_to_cpu(prop+1);

    startup_allocator(mm_start, mm_size, fdt);
    malloc_init();
    print_mm_buddy();

    uart_puts("----------\nTest started!\n\n");
    test_alloc_1();
    while(1);
}
