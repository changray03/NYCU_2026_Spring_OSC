#include "memory.h"
#include "uart.h"
#include "list.h"

unsigned int total_pages; 
unsigned long max_malloc_size;
struct buddy_allocator page_allocator;
struct frame* frames;

struct chunk_pool pools[] = {
    {16,  {0}, 0},
    {32,  {0}, 0},
    {64,  {0}, 0},
    {128,  {0}, 0},
    {256,  {0}, 0},
    {512, {0}, 0},
    {1024, {0}, 0},
    {2048, {0}, 0},
    {0,   {0}, 0} // 結束標誌
};

void print_mm_buddy(){
    uart_puts("Free_list Layout:\n");
    uart_puts("- Buddy System Layout:\n");
    for(int i = 0; i <= MAX_ORDER; i++){
        uart_printf("-- Order %d -> %d\n", i, page_allocator.freelists_size[i]);
    }
    uart_putc('\n');
}

void print_mm_slab(){
    uart_puts("Free_list Layout:\n");
    uart_puts("- Slab System Layout:\n");
    for(int i = 0; pools[i].chunk_size != 0; i++){
        uart_printf("-- Chunk size %d -> %d\n", pools[i].chunk_size, pools[i].freelist_size);
    }
    uart_putc('\n');
}


void buddy_init(unsigned long base) {
    page_allocator.base_addr = base;
    page_allocator.total_pages = total_pages;
    page_allocator.frame_array = frames;

    // 初始化所有 Free Lists (Head 不放值)
    for (int i = 0; i <= MAX_ORDER; i++) {
        page_allocator.freelists_size[i] = 0;
        init(&page_allocator.freelists[i]);
    }

    // 初始化所有 Frame 狀態
    for (int i = 0; i < total_pages; i++) {
        page_allocator.frame_array[i].status = FRAME_ALLOC;
        page_allocator.frame_array[i].order = 0;
        init(&page_allocator.frame_array[i].list_node);
    }
}

static void* buddy_alloc(int order) {
    uart_puts("Buddy System started:\n");
    for (int i = order; i <= MAX_ORDER; i++) {
        if (!list_empty(&page_allocator.freelists[i])) {
            // 找到可用的塊，從 List 中移除
            struct list_head *node = page_allocator.freelists[i].next;
            list_del(node);
            page_allocator.freelists_size[i]--;
            
            struct frame *f = container_of(node, struct frame, list_node); 
            int idx = f - page_allocator.frame_array;
            int i_print = i;
            // 遞迴切分
            while (i != order) {
                // 找到 buddy, 切分成兩塊，將 buddy 放回 Free List
                i--;
                int buddy_idx = idx + (1 << i);
                struct frame *buddy = &page_allocator.frame_array[buddy_idx];
                
                buddy->order = i;
                buddy->status = FRAME_FREE;
                list_add(&buddy->list_node, &page_allocator.freelists[i]);
                page_allocator.freelists_size[i]++;
            }

            f->status = FRAME_ALLOC; // 標記為 <X> 
            f->order = order;
            
            unsigned long addr = page_allocator.base_addr + (idx * PAGE_SIZE);
            if(i != i_print) uart_printf("- Slice block from order %d to %d, addr: %x.\n", i_print, i, addr);
            uart_printf("- Allocate at index %d, addr: 0x%x, order: %d.\n", idx, addr, order);
            return (void*)addr;
        }
    }
    uart_printf("- Allocation failed!\n\n");
    return 0;
}

void buddy_free(void* ptr, int mode) {
    if(mode) uart_puts("Buddy System started:\n");
    int idx = ((unsigned long)ptr - page_allocator.base_addr) / PAGE_SIZE;
    struct frame *f = &page_allocator.frame_array[idx];
    int order = f->order;
    f->status = FRAME_BUDDY;
    f->order = -1;

    if(mode) uart_printf("- Freeing address %x, index %d, order %d\n",(unsigned long)ptr, idx, order);

    // 遞迴合併 
    while (order < MAX_ORDER) {
        int buddy_idx = idx ^ (1 << order); // XOR 找出 Buddy
        
        // 檢查邊界
        if (buddy_idx >= page_allocator.total_pages) break;
        
        struct frame *buddy = &page_allocator.frame_array[buddy_idx];

        // 檢查 Buddy 是否可以合併：必須是 Free 且 Order 相同 
        if (buddy->status == FRAME_FREE && buddy->order == order) {
            if(mode) uart_printf("- Buddy found: index %d, order %d! Merging...\n", buddy_idx, order);
            
            // 將 Buddy 從清單移除
            list_del(&buddy->list_node);
            page_allocator.freelists_size[order] --;
            buddy->status = FRAME_BUDDY; // 標記為 <F> 
            buddy->order = -1;

            // 更新目前區塊索引（取兩者中較小的）
            if (buddy_idx < idx) idx = buddy_idx;
            order++;
        } else {
            break; // 無法合併，跳出
        }
    }

    // 將合併後的最終大塊放回清單
    struct frame *final_f = &page_allocator.frame_array[idx];
    final_f->order = order;
    final_f->status = FRAME_FREE;
    list_add(&final_f->list_node, &page_allocator.freelists[order]);
    page_allocator.freelists_size[order]++;
    if(mode) uart_printf("- Add back to order %d at index %d\n", order, idx);
}

void malloc_init() {
    for (int i = 0; pools[i].chunk_size != 0; i++) {
        init(&pools[i].free_list);
        pools[i].freelist_size = 0;
    }
}

void* allocate(unsigned long size) {
    // 超過一頁，交給 Buddy System
    if (size > 2048) {
        int order = 0;
        while ((PAGE_SIZE << order) < size) order++;    // 找到能夠放得下 size 的 order
        void* temp = buddy_alloc(order);
        print_mm_buddy();
        return temp;
    }
    
    // 尋找合適的池子 
    struct chunk_pool *pool = 0;
    for (int i = 0; pools[i].chunk_size != 0; i++) {
        if (size <= pools[i].chunk_size) {
            pool = &pools[i];
            break;
        }
    }
    uart_printf("Slab System started:\n");
    // 如果池子空了，申請新頁面並切割 
    if (list_empty(&pool->free_list)) {
        uart_printf("- Chunk size %d is empty, activate Buddy System.\n- ", pool->chunk_size);
        void *page_addr = buddy_alloc(0); // 拿 4KB
        if (!page_addr) return 0;
        print_mm_buddy();
        uart_printf("- Successfully get a page from Buddy System!\n");
        int idx = ((unsigned long)page_addr - page_allocator.base_addr)/PAGE_SIZE;
        struct frame *f = &page_allocator.frame_array[idx];
        f->status = pool->chunk_size;   // 當要執行 free 時才知道是 buddy system or slab system 分配的

        // 將頁面切割成 chunks 
        int num_chunks = PAGE_SIZE / pool->chunk_size;
        for (int i = 0; i < num_chunks; i++) {
            struct list_head *chunk = (struct list_head *)((char *)page_addr + (i * pool->chunk_size));
            list_add_tail(chunk, &pool->free_list);
            pool->freelist_size++;
        }
    }

    // 從池子取出一個 chunk 回傳 
    struct list_head *node = pool->free_list.next;
    list_del(node);
    pool->freelist_size--;
    
    uart_printf("- Allocate chunk size %d at %x.\n", pool->chunk_size, (unsigned long)node);
    print_mm_slab();
    return (void*)node;
}

void free(void* ptr) {
    if (!ptr){
        uart_printf("Illegal address!\n\n");
        return;
    }

    // 找到對應的 Frame
    int idx = ((unsigned long)ptr - page_allocator.base_addr) / PAGE_SIZE;
    struct frame *f = &page_allocator.frame_array[idx];

    // 判斷是 Page 還是 Chunk
    if (f->status == FRAME_ALLOC) { 
        // 這是透過 buddy_alloc(order) 直接給出的整塊頁面
        buddy_free(ptr, 1);
        print_mm_buddy();
        return;
    } 
    else if (f->status >= 16) { 
        // 這是一頁被切成小塊的頁面，f->status 存著當初切的大小（如 16, 32...）
        int size = f->status;
        
        // 尋找對應的池子
        for (int i = 0; pools[i].chunk_size != 0; i++) {
            if (pools[i].chunk_size == size) {
                struct list_head *node = (struct list_head *)ptr;
                list_add(node, &pools[i].free_list); // 還給池子
                pools[i].freelist_size++;
                uart_printf("- Free %x back to chunk size of %d.\n", (unsigned long)ptr, size);
                break;
            }
        }
    }
    else{
        uart_printf("Illegal address!\n\n");
    }
    print_mm_slab();
}