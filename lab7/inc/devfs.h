#ifndef DEVFS_H
#define DEVFS_H

#include "vfs.h"

#define DEVFS_MAX_NAME 16
#define DEVFS_MAX_ENTRIES 16
#define DEVFS_MAX_FILE_SIZE 4096

struct devfs_node {
    char name[DEVFS_MAX_NAME];  
    enum { DEVFS_DIR, DEVFS_FILE } type;
    unsigned long size;            
    union {
        char* data;                 
        struct vnode* entries[DEVFS_MAX_ENTRIES]; 
    };
};

extern struct filesystem devfs_driver;

#endif