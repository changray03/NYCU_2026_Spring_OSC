#ifndef LIST_H
#define LIST_H
#include<stddef.h>
/**
 * 將此結構嵌入到 struct 中（struct frame）。
 */
struct list_head {
    struct list_head *next, *prev;
};

/**
 * 初始化一個空鏈結串列，讓 next 和 prev 都指向自己。
 */
static inline void init(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

/**
 * 內部函數：將 new 插入在 prev 與 next 之間。
 */
static inline void __list_add(struct list_head *new_node,
                              struct list_head *prev,
                              struct list_head *next) {
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

/**
 * list_add - 插入到鏈結串列的開頭
 */
static inline void list_add(struct list_head *new_node, struct list_head *head) {
    __list_add(new_node, head, head->next);
}

/**
 * list_add_tail - 插入到鏈結串列的末尾
 */
static inline void list_add_tail(struct list_head *new_node, struct list_head *head) {
    __list_add(new_node, head->prev, head);
}

/**
 * list_del - 從鏈結串列中移除節點
 */
static inline void list_del(struct list_head *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    entry->next = 0; // 安全起見，清空指標
    entry->prev = 0;
}

/**
 * list_empty - 檢查鏈結串列是否為空
 */
static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}

/**
 * offsetof & container_of 
 * 用來從 list_head 指標計算回父結構（如 struct frame）的起始位址。
 */
//#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
    const __typeof__( ((type*)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

#endif /* LIST_H */