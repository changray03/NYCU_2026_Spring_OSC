#ifndef MM_H
#define MM_H

#include "list.h"
#include <stdint.h>
//#include "paging.h"

#define MAX_ORDER 10
//#define TOTAL_PAGES 256
//#define MAX_ALLOC_SIZE 0x00100000

// 定義 Frame 的狀態
#define FRAME_FREE    0   // 代表 val >= 0
#define FRAME_ALLOC   -1  // 代表 <X>
#define FRAME_BUDDY   -2  // 代表 <F>
#define FRAME_RESERVED -3 // 代表 reserved region

extern unsigned int total_pages;
extern unsigned long max_malloc_size;
extern struct frame* frames;

typedef struct {
    uint64_t start;
    uint64_t end;
} reserved_region_t;

// page frame 的資訊
struct frame {
    int order;
    int status;
    struct list_head list_node;
};

struct buddy_allocator {
    struct frame *frame_array;
    struct list_head freelists[MAX_ORDER + 1];
    int freelists_size[MAX_ORDER + 1];
    unsigned long base_addr;
    unsigned int total_pages;
};

struct chunk_pool {
    unsigned int chunk_size;
    struct list_head free_list; // 掛載該尺寸的所有空閒 chunk
    int freelist_size;
};

extern struct buddy_allocator page_allocator;
extern struct chunk_pool pools[];

void print_mm_buddy();
void print_mm_slab();
void buddy_init(unsigned long base);
void malloc_init();
void* allocate(unsigned long size);
void free(void* ptr);
void buddy_free(void* ptr, int mode);
void startup_allocator(uint64_t start, uint64_t size, uint64_t fdt);
void memory_init(uint64_t fdt);


#endif