#include "vfs.h"
#include "memory.h"
#include "utils.h"
#include "uart.h"
#include "devfs.h"

#define SEET_SET 0
#define FB_IOCTL_GET_INFO 0

// fb setting
#ifdef DEBUG
    #define FB_BASE   0xffffffc087000000UL
#else
    #define FB_BASE   0xffffffc07f700000UL
#endif
#define FB_WIDTH  1920
#define FB_HEIGHT 1080
#define FB_BPP    4
struct framebuffer_info {
    unsigned int width;
    unsigned int height;
    unsigned int bpp;
};

int devfs_setup_mount(struct filesystem* fs, struct mount* mount);
int devfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name);
int devfs_uart_read(struct file* file, void* buf, size_t len);
int devfs_uart_write(struct file* file, const void* buf, size_t len);
int devfs_fb_ioctl(struct file* file, unsigned long request, void* args);
int devfs_fb_write(struct file* file, const void* buf, size_t len);
long devfs_fb_lseek64(struct file* file, long offset, int whence);

// UART 專屬的檔案操作
struct file_operations devfs_uart_f_ops = {
    .open = 0, .close = 0,
    .read = devfs_uart_read,
    .write = devfs_uart_write
};

struct file_operations devfs_fb_f_ops = {
    .open    = 0,
    .close   = 0,
    .read    = 0,
    .write   = devfs_fb_write,
    .lseek64 = devfs_fb_lseek64,
    .ioctl   = devfs_fb_ioctl   // <--- 掛上 ioctl 控制器！
};

struct vnode_operations devfs_v_ops = {
    .lookup = devfs_lookup, .create = 0, .mkdir = 0
};

struct filesystem devfs_driver = {
    .name = "devfs", .setup_mount = devfs_setup_mount
};

int devfs_setup_mount(struct filesystem* fs, struct mount* mount) {
    //  建立 devfs 的 root
    struct vnode* root_vnode = (struct vnode*)allocate(sizeof(struct vnode));
    root_vnode->mount = 0; 
    root_vnode->v_ops = &devfs_v_ops; 
    root_vnode->f_ops = 0;
    root_vnode->parent = root_vnode; 
    root_vnode->type = VFS_TYPE_DIR;
    struct devfs_node* d_root = (struct devfs_node*)allocate(sizeof(struct devfs_node));
    d_root->type = DEVFS_DIR; 
    strncpy(d_root->name, "/", 2);
    d_root->size = 0;
    memset(d_root->entries, 0, sizeof(d_root->entries)); 

    // 預先建立 uart 設備檔案節點
    struct vnode* uart_vnode = (struct vnode*)allocate(sizeof(struct vnode));
    uart_vnode->mount = 0; 
    uart_vnode->v_ops = &devfs_v_ops; 
    uart_vnode->f_ops = &devfs_uart_f_ops;
    uart_vnode->parent = root_vnode; 
    uart_vnode->type = VFS_TYPE_FILE;
    struct devfs_node* u_node = (struct devfs_node*)allocate(sizeof(struct devfs_node));
    u_node->type = DEVFS_FILE; 
    strncpy(u_node->name, "uart", 5);
    u_node->size = 0;
    memset(u_node->entries, 0, sizeof(u_node->entries)); 
    uart_vnode->internal = u_node;
    d_root->entries[0] = uart_vnode;

    // 預先建立 fb 設備檔案節點
    struct vnode* fb_vnode = (struct vnode*)allocate(sizeof(struct vnode));
    fb_vnode->mount = 0; 
    fb_vnode->v_ops = &devfs_v_ops; 
    fb_vnode->f_ops = &devfs_fb_f_ops;
    fb_vnode->parent = root_vnode; 
    fb_vnode->type = VFS_TYPE_FILE;
    struct devfs_node* f_node = (struct devfs_node*)allocate(sizeof(struct devfs_node));
    f_node->type = DEVFS_FILE; 
    strncpy(f_node->name, "fb", 3);
    f_node->size = 0;
    memset(f_node->entries, 0, sizeof(f_node->entries)); 
    fb_vnode->internal = f_node;
    d_root->entries[1] = fb_vnode;

    root_vnode->internal = d_root;
    mount->root = root_vnode; 
    mount->fs = fs;

    extern void video_init();
    #ifdef DEBUG
        video_init();
    #endif
    return 0;
}

int devfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    struct devfs_node* d_node = (struct devfs_node*)dir_node->internal;
    if (d_node->type != DEVFS_DIR) return -1; 

    for (int i = 0; i < DEVFS_MAX_ENTRIES; i++) {  
        if (d_node->entries[i] != 0) {
            struct devfs_node* child = (struct devfs_node*)d_node->entries[i]->internal;
            if (strcmp(child->name, component_name) == 0) {
                *target = d_node->entries[i];   
                return 0;
            }
        }
    }
    return -1;
}

// 實質對接核心 UART I/O
int devfs_uart_read(struct file* file, void* buf, size_t len) {
    char* c_buf = (char*)buf;
    for (size_t i = 0; i < len; i++) {
        c_buf[i] = uart_getc();
    }
    return len;
}

int devfs_uart_write(struct file* file, const void* buf, size_t len) {
    const char* c_buf = (const char*)buf;
    for (size_t i = 0; i < len; i++) {
        uart_putc(c_buf[i]);
    }
    return len;
}

int devfs_fb_write(struct file* file, const void* buf, size_t len) {
    // 邊界檢查，若是寫入的 len 超過 fb 的區域則取最大能寫入之 len
    if (file->f_pos + len > (FB_WIDTH * FB_HEIGHT * FB_BPP)) {
        len = (FB_WIDTH * FB_HEIGHT * FB_BPP) - file->f_pos;
    }
    if (len <= 0) return 0;

    // 算出精準的寫入位址
    unsigned char* dest = (unsigned char*)FB_BASE + file->f_pos;
    memcpy(dest, buf, len);

    // 將 memcpy 複製的 data flush 回 memory(原可能在 dcache)，如此 display controller 才能讀到最新的 data
    extern void flush_dcache(void* addr, unsigned long len);
    flush_dcache(dest, len);

    file->f_pos += len;
    return len;
}

long devfs_fb_lseek64(struct file* file, long offset, int whence) {
    if (whence == SEET_SET) { // SEEK_SET (以檔案開頭為基準)
        if (offset < 0 || offset > (FB_WIDTH * FB_HEIGHT * FB_BPP)) return -1;
        file->f_pos = offset;
        return file->f_pos;
    }
    return -1; // 本實驗只需要支援 SEEK_SET
}

int devfs_fb_ioctl(struct file* file, unsigned long request, void* args) {
    if (request == FB_IOCTL_GET_INFO) { // FB_IOCTL_GET_INFO
        struct framebuffer_info* fb_user = (struct framebuffer_info*)args;
        if (!fb_user) return -1;

        // 回報真正的硬體規格給 U-mode 程式
        fb_user->width = FB_WIDTH;
        fb_user->height = FB_HEIGHT;
        fb_user->bpp = FB_BPP;
        return 0;
    }
    return -1;
}