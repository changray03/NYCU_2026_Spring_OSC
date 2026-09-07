#ifndef VFS_H
#define VFS_H

#include <stddef.h>

#define O_CREAT 00000100
#define MAX_FD 16
#define MAX_NAME 16
#define MAX_ENTRIES 16
#define MAX_FILE_SIZE 4096

enum vnode_type {
    VFS_TYPE_DIR,
    VFS_TYPE_FILE
};

struct vnode {
    struct mount* mount;
    struct vnode_operations* v_ops;
    struct file_operations* f_ops;
    struct vnode* parent;
    enum vnode_type type;
    void* internal;
};

struct file {
    struct vnode* vnode;
    size_t f_pos;  
    struct file_operations* f_ops;
    int ref_count;
    int flags;
};

struct mount {
    struct vnode* root;
    struct filesystem* fs;
    struct vnode* mountpoint;
};

struct filesystem {
    const char* name;
    int (*setup_mount)(struct filesystem* fs, struct mount* mount);
};

struct file_operations {
    int (*open)(struct vnode* file_node, struct file** target);
    int (*close)(struct file* file);
    int (*read)(struct file* file, void* buf, size_t len);
    int (*write)(struct file* file, const void* buf, size_t len);
    long (*lseek64)(struct file* file, long offset, int whence);
    int (*ioctl)(struct file* file, unsigned long request, void* args);
};

struct vnode_operations {
    int (*lookup)(struct vnode* dir_node, struct vnode** target, const char* component_name);
    int (*create)(struct vnode* dir_node, struct vnode** target, const char* component_name);
    int (*mkdir)(struct vnode* dir_node, struct vnode** target, const char* component_name);
};

struct fd_table {
    struct file* fds[MAX_FD];
};

extern struct mount* rootfs;
extern struct mount* active_mounts[8];
extern int active_mount_count;

// VFS 全域 API
int register_filesystem(struct filesystem* fs);
int vfs_open(const char* pathname, int flags, struct file** target);
int vfs_close(struct file* file);
int vfs_read(struct file* file, void* buf, size_t len);
int vfs_write(struct file* file, const void* buf, size_t len);
int vfs_mkdir(const char* pathname);
int vfs_mount(const char* target, const char* filesystem);
int vfs_lookup(const char* pathname, struct vnode** target);
int vfs_chdir(struct vnode** current_cwd, const char* pathname) ;
int vfs_ioctl(struct file* file, unsigned long request, void* args);
long vfs_lseek64(struct file* file, long offset, int whence);
#endif