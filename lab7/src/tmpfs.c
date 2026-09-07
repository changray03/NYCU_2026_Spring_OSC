#include "vfs.h"
#include "memory.h"
#include "utils.h"
#include "tmpfs.h"  

struct vnode_operations tmpfs_v_ops = { .lookup = tmpfs_lookup, .create = tmpfs_create, .mkdir = tmpfs_mkdir };
struct file_operations  tmpfs_f_ops = { .open = 0, .close = 0, .read = tmpfs_read, .write = tmpfs_write, .lseek64 = 0, .ioctl = 0 };
struct filesystem tmpfs_driver = { .name = "tmpfs", .setup_mount = tmpfs_setup_mount };  

int tmpfs_setup_mount(struct filesystem* fs, struct mount* mount) {
    // 爲 root 建立新的 vnode
    struct vnode* root_vnode = (struct vnode*)allocate(sizeof(struct vnode));
    if (!root_vnode) return -1;
    root_vnode->mount = 0; 
    root_vnode->v_ops = &tmpfs_v_ops;
    root_vnode->f_ops = &tmpfs_f_ops;
    root_vnode->parent = root_vnode;
    root_vnode->type = VFS_TYPE_DIR;

    // 爲 root 建立新的 tmpfs node
    struct tmpfs_node* t_root = (struct tmpfs_node*)allocate(sizeof(struct tmpfs_node));
    if (!t_root) return -1;
    t_root->type = TMPFS_DIR;
    t_root->size = 0;
    memset(t_root->entries, 0, sizeof(t_root->entries)); 
    //t_root->parent = root_vnode;

    root_vnode->internal = t_root;  
    mount->root = root_vnode;       
    mount->fs = fs;                 
    return 0;
}

int tmpfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    struct tmpfs_node* d_node = (struct tmpfs_node*)dir_node->internal;
    if (d_node->type != TMPFS_DIR) return -1; 

    for (int i = 0; i < TMPFS_MAX_ENTRIES; i++) {  
        if (d_node->entries[i] != 0) {
            struct tmpfs_node* child = (struct tmpfs_node*)d_node->entries[i]->internal;
            if (strcmp(child->name, component_name) == 0) {
                *target = d_node->entries[i];   
                return 0;
            }
        }
    }
    return -1;   
}

int tmpfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    struct tmpfs_node* d_node = (struct tmpfs_node*)dir_node->internal;
    if (d_node->type != TMPFS_DIR) return -1;

    // 若是檔案已存在，則建立失敗
    struct vnode* dummy;
    if (tmpfs_lookup(dir_node, &dummy, component_name) == 0) return -1;

    // 找到目前 directory 中 free 的 entry 
    int free_slot = -1;
    for (int i = 0; i < TMPFS_MAX_ENTRIES; i++) {  
        if (d_node->entries[i] == 0) {
            free_slot = i;
            break;
        }
    }
    if (free_slot == -1) return -1; 

    // 爲檔案建立新的 vnode
    struct vnode* new_vnode = (struct vnode*)allocate(sizeof(struct vnode));
    if (!new_vnode) return -1;
    new_vnode->mount = 0; // 繼承或維持 0，非掛載點
    new_vnode->v_ops = &tmpfs_v_ops;
    new_vnode->f_ops = &tmpfs_f_ops;
    new_vnode->parent = dir_node;
    new_vnode->type = VFS_TYPE_FILE;

    // 爲檔案建立新的 tmpfs node
    struct tmpfs_node* new_t_node = (struct tmpfs_node*)allocate(sizeof(struct tmpfs_node));
    if (!new_t_node) return -1;
    new_t_node->type = TMPFS_FILE;
    new_t_node->size = 0;
    new_t_node->data = (char*)allocate(TMPFS_MAX_FILE_SIZE); 
    if (!new_t_node->data) return -1;
    //new_t_node->parent = dir_node;
    // 取得 file name
    int i = 0;
    for (; i < TMPFS_MAX_NAME && component_name[i] != '\0'; i++) {  
        new_t_node->name[i] = component_name[i];
    }
    new_t_node->name[i] = '\0';

    new_vnode->internal = new_t_node;  
    d_node->entries[free_slot] = new_vnode; 
    
    *target = new_vnode; 
    return 0;
}

// 建立子目錄操作
int tmpfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    struct tmpfs_node* d_node = (struct tmpfs_node*)dir_node->internal;
    if (d_node->type != TMPFS_DIR) return -1;

    // 同名檢查，若已存在則創建失敗
    struct vnode* dummy;
    if (tmpfs_lookup(dir_node, &dummy, component_name) == 0) return -1;

    // 尋找空槽位
    int free_slot = -1;
    for (int i = 0; i < TMPFS_MAX_ENTRIES; i++) {
        if (d_node->entries[i] == 0) {
            free_slot = i;
            break;
        }
    }
    if (free_slot == -1) return -1;

    // 配置抽象 vnode 節點
    struct vnode* new_vnode = (struct vnode*)allocate(sizeof(struct vnode));
    if (!new_vnode) return -1;
    new_vnode->mount = 0; 
    new_vnode->v_ops = &tmpfs_v_ops;
    new_vnode->f_ops = &tmpfs_f_ops;
    new_vnode->parent = dir_node;
    new_vnode->type = VFS_TYPE_DIR;

    // 配置具體 tmpfs 目錄節點
    struct tmpfs_node* new_t_node = (struct tmpfs_node*)allocate(sizeof(struct tmpfs_node));
    if (!new_t_node) return -1;
    new_t_node->type = TMPFS_DIR;
    new_t_node->size = 0;
    //new_t_node->parent = dir_node;
    memset(new_t_node->entries, 0, sizeof(new_t_node->entries)); // 清空新目錄

    // 取得 directory name
    int i = 0;
    for (; i < TMPFS_MAX_NAME && component_name[i] != '\0'; i++) {
        new_t_node->name[i] = component_name[i];
    }
    new_t_node->name[i] = '\0';

    new_vnode->internal = new_t_node;
    d_node->entries[free_slot] = new_vnode;

    *target = new_vnode;
    return 0;
}

int tmpfs_read(struct file* file, void* buf, unsigned long len) {
    struct tmpfs_node* t_node = (struct tmpfs_node*)file->vnode->internal;
    if (t_node->type != TMPFS_FILE) return -1;

    // 若是已讀到檔案末尾，return 0 表示讀完了
    if (file->f_pos >= t_node->size) return 0;  

    // 邊界檢查，如果讀取大小超過最大檔案大小，則把 len 截斷取到能讀的最大
    unsigned long available = t_node->size - file->f_pos;
    if (len > available) len = available; 

    memcpy(buf, t_node->data + file->f_pos, len);
    
    file->f_pos += len;  
    return len;  
}

int tmpfs_write(struct file* file, const void* buf, unsigned long len) {
    struct tmpfs_node* t_node = (struct tmpfs_node*)file->vnode->internal;
    if (t_node->type != TMPFS_FILE) return -1;

    // 邊界檢查，如果寫入大小超過最大檔案大小，則把 len 截斷取到能寫的最大
    if (file->f_pos + len > TMPFS_MAX_FILE_SIZE) {  
        len = TMPFS_MAX_FILE_SIZE - file->f_pos;
    }
    if (len == 0) return -1;

    // 從 file->f_pos 的位置開始寫入 len 的 data
    memcpy(t_node->data + file->f_pos, buf, len);
    
    file->f_pos += len;  
    
    // 調整檔案大小
    if (file->f_pos > t_node->size) {
        t_node->size = file->f_pos;
    }
    return len;
}