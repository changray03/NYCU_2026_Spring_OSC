#ifndef RAMFS_H
#define RAMFS_H

#include "vfs.h"

#define RAMFS_MAX_NAME 16
#define RAMFS_MAX_ENTRIES 16
#define RAMFS_MAX_FILE_SIZE 4096

// 唯讀檔案系統內部結構的簡化版根節點
struct ramfs_node {
    char name[RAMFS_MAX_NAME];
    enum { RAMFS_DIR, RAMFS_FILE } type;
    unsigned long size;            
    union {
        char* data;                 
        struct vnode* entries[RAMFS_MAX_ENTRIES]; 
    };
};

// 宣告 ramfs 的檔案系統驅動結構體，供外部（如 main）註冊使用
extern struct filesystem ramfs_driver;

// 宣告 ramfs 的專屬操作介面
int ramfs_setup_mount(struct filesystem* fs, struct mount* mount);
int ramfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name);
int ramfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name);
int ramfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name);
int ramfs_read(struct file* file, void* buf, size_t len);
int ramfs_write(struct file* file, const void* buf, size_t len);

#endif