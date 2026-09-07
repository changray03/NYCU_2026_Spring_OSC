#include "ramfs.h"
#include "memory.h"
#include "utils.h"
#include "cpio.h"
#include "uart.h"

// 綁定 ramfs 的操作函數
struct vnode_operations ramfs_v_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create, // 唯讀，此操作將會失敗
    .mkdir  = ramfs_mkdir   // 唯讀，此操作將會失敗
};

struct file_operations ramfs_f_ops = {
    .open    = 0,
    .close   = 0,
    .read    = ramfs_read,
    .write   = ramfs_write, // 唯讀，此操作將會失敗
    .lseek64 = 0,
    .ioctl = 0
};

// 定義符合 VFS 規範的 filesystem 驅動
struct filesystem ramfs_driver = {
    .name = "ramfs",
    .setup_mount = ramfs_setup_mount
};

int ramfs_setup_mount(struct filesystem* fs, struct mount* mount) {
    // 分配抽象 vnode
    struct vnode* root_vnode = (struct vnode*)allocate(sizeof(struct vnode));
    if (!root_vnode) return -1;
    root_vnode->mount = 0;
    root_vnode->v_ops = &ramfs_v_ops;
    root_vnode->f_ops = &ramfs_f_ops;
    root_vnode->parent = root_vnode; // 根目錄的 parent 指向自己以利回溯
    root_vnode->type = VFS_TYPE_DIR;

    // 分配 ramfs 內部的根節點
    struct ramfs_node* r_root = (struct ramfs_node*)allocate(sizeof(struct ramfs_node));
    if (!r_root) {
        free(root_vnode);
        return -1;
    }
    r_root->type = RAMFS_DIR; // 標記為目錄
    strncpy(r_root->name, "/", 2);
    r_root->size = 0;
    memset(r_root->entries, 0, sizeof(r_root->entries)); 

    // 將 cpio 內的檔案加入到 /ramfs 下
    void* current_file_hdr = NULL; // 初始傳入 NULL 代表從頭出發
    const char* filename = NULL;
    uint64_t filesize = 0;
    void* next_file_hdr = NULL;
    void* actual_data = NULL;
    int slot_idx = 0;

    while ((actual_data = cpio_get_all_files(current_file_hdr, &filename, &filesize, &next_file_hdr)) != NULL) {
        if (slot_idx >= RAMFS_MAX_ENTRIES) {
            uart_puts("Warning: ramfs root directory entries full!\n");
            break; 
        }

        // 跳過特殊的目前目錄 "." 標記，防止路徑死循環（CPIO 封裝時有時會帶有目錄標記）
        if (strcmp(filename, ".") == 0) {
            current_file_hdr = next_file_hdr;
            continue;
        }

        struct vnode* file_vnode = (struct vnode*)allocate(sizeof(struct vnode));
        struct ramfs_node* file_node = (struct ramfs_node*)allocate(sizeof(struct ramfs_node));
        
        if (file_vnode && file_node) {
            memset(file_node, 0, sizeof(struct ramfs_node));
            file_node->type = RAMFS_FILE;
            file_node->size = filesize;        // 精準對齊真實大小
            file_node->data = (char*)actual_data; // 指標零拷貝直對 CPIO 實體記憶體
            strncpy(file_node->name, filename, RAMFS_MAX_NAME - 1);

            file_vnode->mount = 0;
            file_vnode->v_ops = &ramfs_v_ops;
            file_vnode->f_ops = &ramfs_f_ops;
            file_vnode->parent = root_vnode;
            file_vnode->type = VFS_TYPE_FILE;
            file_vnode->internal = file_node;

            // 將建立好的 vnode 塞入當前目錄的 entries 控制槽中
            r_root->entries[slot_idx++] = file_vnode;
        }

        // 指標向後推，準備處理下一個檔案
        current_file_hdr = next_file_hdr;
    }

    root_vnode->internal = r_root;
    mount->root = root_vnode;
    mount->fs = fs;
    return 0;
}

// 由於是唯讀且初始為空，lookup 找不到任何子檔案/子目錄，直接回傳失敗
int ramfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    struct ramfs_node* d_node = (struct ramfs_node*)dir_node->internal;
    if (d_node->type != RAMFS_DIR) return -1; 

    for (int i = 0; i < RAMFS_MAX_ENTRIES; i++) {  
        if (d_node->entries[i] != 0) {
            struct ramfs_node* child = (struct ramfs_node*)d_node->entries[i]->internal;
            if (strcmp(child->name, component_name) == 0) {
                *target = d_node->entries[i];   
                return 0;
            }
        }
    }
    return -1; // 找不到該項目
}

int ramfs_read(struct file* file, void* buf, size_t len) {
    struct ramfs_node* t_node = (struct ramfs_node*)file->vnode->internal;
    if (t_node->type == RAMFS_DIR) {
        return 0; 
    }
    if (t_node->type != RAMFS_FILE) return -1;

    // 若是已讀到檔案末尾，return 0 表示讀完了 (EOF)
    if (file->f_pos >= t_node->size) return 0;  

    // 邊界檢查，確保不會越界讀取
    size_t available = t_node->size - file->f_pos;
    if (len > available) len = available; 

    // 從記憶體中複製資料到使用者的 buffer
    memcpy(buf, t_node->data + file->f_pos, len);
    
    file->f_pos += len;  
    return len;  
}

int ramfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    (void)dir_node; (void)target; (void)component_name;
    return -1; 
}

int ramfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    (void)dir_node; (void)target; (void)component_name;
    return -1; 
}

int ramfs_write(struct file* file, const void* buf, size_t len) {
    (void)file; (void)buf; (void)len;
    return -1; 
}