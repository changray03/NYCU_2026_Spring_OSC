#ifndef TMPFS_H
#define TMPFS_H

#include "vfs.h"

#define TMPFS_MAX_NAME 16
#define TMPFS_MAX_ENTRIES 16
#define TMPFS_MAX_FILE_SIZE 4096

struct tmpfs_node {
    char name[TMPFS_MAX_NAME];  
    enum { TMPFS_DIR, TMPFS_FILE } type;
    unsigned long size;            
    union {
        char* data;                 
        struct vnode* entries[TMPFS_MAX_ENTRIES]; 
    };
};

extern struct filesystem tmpfs_driver;

int tmpfs_setup_mount(struct filesystem* fs, struct mount* mount);  
int tmpfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name); 
int tmpfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name);
int tmpfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name);
int tmpfs_read(struct file* file, void* buf, unsigned long len);  
int tmpfs_write(struct file* file, const void* buf, unsigned long len);

#endif