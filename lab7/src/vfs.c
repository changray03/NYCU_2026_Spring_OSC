#include "vfs.h"
#include "utils.h"
#include "memory.h"
#include "sched.h" // 引入以獲取當前進程結構
#include "tmpfs.h"
#include "uart.h"

struct mount* rootfs;

static struct filesystem* registered_filesystems[8];
static int registered_count = 0;

struct mount* active_mounts[8];
int active_mount_count = 0;

int register_filesystem(struct filesystem* fs) {
    if (registered_count >= 8) return -1;
    registered_filesystems[registered_count++] = fs;
    return 0;
}

// 支援相對路徑與特殊組件的進階路徑解析核心
int vfs_lookup_from(struct vnode* base_dir, const char* pathname, struct vnode** target) {
    if (!pathname || pathname[0] == '\0') return -1;

    // 預設使用傳入的 base
    struct vnode* vnode_itr = base_dir;
    int path_idx = 0;

    // 判斷是否為絕對路徑，若是則從 rootfs->root 開始
    if (pathname[0] == '/') {
        vnode_itr = rootfs->root;
        // 跳過開頭所有連續的 '/'
        while (pathname[path_idx] == '/') {
            path_idx++;
        }
    }

    char component[MAX_NAME];

    while (pathname[path_idx] != '\0') {
        int comp_idx = 0;
        // 剝離出當前層級的組件名稱
        while (pathname[path_idx] != '/' && pathname[path_idx] != '\0') {
            if (comp_idx < MAX_NAME - 1) {
                component[comp_idx++] = pathname[path_idx];
            }
            path_idx++;
        }
        component[comp_idx] = '\0';

        // 跳過連續的斜線 '/'
        while (pathname[path_idx] == '/') {
            path_idx++;
        }

        // 如果組件為空（例如末尾多餘的斜線），直接結束
        if (comp_idx == 0) break;

        // 處理特殊路徑組件 "." 與 ".."
        if (strcmp(component, ".") == 0) {
            continue; 
        } 
        else if (strcmp(component, "..") == 0) {
            if (vnode_itr == rootfs->root) continue;

            // 進入向上彈出掛載點的迴圈
            while (1) {
                struct mount* target_mnt = NULL;
                // 檢查目前的 vnode_itr 是不是某個被掛載系統的根目錄
                uint64_t s = disable_and_save_sstatus();
                for (int m = 0; m < active_mount_count; m++) {
                    if (active_mounts[m]->root == vnode_itr) {
                        target_mnt = active_mounts[m];
                        break;
                    }
                }
                restore_sstatus(s);

                // 如果它是某個系統的根目錄，就利用 mountpoint 彈回外層檔案系統
                if (target_mnt) {
                    vnode_itr = target_mnt->mountpoint;
                    continue; 
                }
                // 如果它已經不是任何系統的根目錄了，說明已經來到某個檔案系統內部的常規目錄
                break;
            }

            // 已經在某個檔案系統的常規目錄下了，拿它的 parent 完成 ..
            if (vnode_itr != rootfs->root) {
                vnode_itr = vnode_itr->parent ? vnode_itr->parent : vnode_itr;
            }

            // 如果下一個節點是掛載點，跨越掛載邊界進入新檔案系統的根目錄
            while (vnode_itr && vnode_itr->mount != 0) {
                vnode_itr = vnode_itr->mount->root;
            }
            continue;
        }

        // 呼叫當前目錄驅動的 lookup
        struct vnode* next_vnode = 0;
        if (!vnode_itr->v_ops || !vnode_itr->v_ops->lookup) return -1;
        
        int ret = vnode_itr->v_ops->lookup(vnode_itr, &next_vnode, component);
        if (ret != 0) return ret; // 找不到該層級

        // 關鍵檢查：如果下一個節點是掛載點，跨越掛載邊界進入新檔案系統的根目錄
        while (next_vnode && next_vnode->mount != 0) {
            next_vnode = next_vnode->mount->root;
        }

        vnode_itr = next_vnode;
    }

    *target = vnode_itr;
    return 0;
}

/*
// 保持向下相容的舊版全域 API（預設由目前進程的 cwd 出發）
int vfs_lookup(const char* pathname, struct vnode** target) {
    struct task_struct* curr = get_current();
    struct vnode* base = (curr && curr->cwd) ? curr->cwd : rootfs->root;
    return vfs_lookup_from(base, pathname, target);
}
*/

// 基於任務 CWD 實現的 vfs_open
int vfs_open(const char* pathname, int flags, struct file** target) {
    struct vnode* found_node = 0;
    struct task_struct* curr = get_current();
    struct vnode* base = (curr && curr->cwd) ? curr->cwd : rootfs->root;
    
    int ret = vfs_lookup_from(base, pathname, &found_node);
    
    // 如果沒找到，且指定了 O_CREAT，則需要建立新檔案
    if (ret != 0 && (flags & O_CREAT)) {
        int last_slash = -1;
        for (int i = 0; pathname[i] != '\0'; i++) {
            if (pathname[i] == '/') last_slash = i;
        }
        
        struct vnode* parent_vnode = 0;
        char filename[MAX_NAME];

        if (last_slash == -1) {
            // 純檔名，父目錄即為目前的 base 目域
            parent_vnode = base;
            strncpy(filename, pathname, MAX_NAME);
        } 
        else {
            // 分離出父目錄路徑
            char parent_path[256];
            int p_idx = 0;
            for (; p_idx <= last_slash; p_idx++) {
                parent_path[p_idx] = pathname[p_idx];
            }
            if (last_slash > 0) parent_path[last_slash] = '\0';
            else parent_path[1] = '\0';

            // 分離出新檔名
            int f_idx = 0;
            for (int i = last_slash + 1; pathname[i] != '\0'; i++) {
                if (f_idx < MAX_NAME - 1) {
                    filename[f_idx++] = pathname[i];
                }
            }
            filename[f_idx] = '\0';

            // 取得父目錄的 vnode
            ret = vfs_lookup_from(base, parent_path, &parent_vnode);
            if (ret != 0) return ret;
        }

        // 呼叫 fs api 完成 vnode 和 interal 的建立
        if (!parent_vnode->v_ops || !parent_vnode->v_ops->create) return -1;
        ret = parent_vnode->v_ops->create(parent_vnode, &found_node, filename);
        if (ret != 0) return ret;
    } 
    else if (ret != 0) {
        return ret; // 檔案不存在且未設置 O_CREAT
    }

    // 建立 file struct 讓 task_struct 記錄檔案
    struct file* h = (struct file*)allocate(sizeof(struct file));
    if (!h) return -1;
    h->vnode = found_node;
    h->f_pos = 0;
    h->f_ops = found_node->f_ops;
    h->ref_count = 1;
    h->flags = flags;

    *target = h;
    return 0;
}

// 基於任務 CWD 實現的 vfs_mkdir
int vfs_mkdir(const char* pathname) {
    int last_slash = -1;
    for (int i = 0; pathname[i] != '\0'; i++) {
        if (pathname[i] == '/') last_slash = i;
    }

    struct task_struct* curr = get_current();
    struct vnode* base = (curr && curr->cwd) ? curr->cwd : rootfs->root;
    struct vnode* parent_vnode = 0;
    char dirname[MAX_NAME];

    // 純目錄名，父目錄即為目前的 base 目域
    if (last_slash == -1) {
        parent_vnode = base;
        strncpy(dirname, pathname, MAX_NAME);
    } 
    else {
        // 分離出父目錄
        char parent_path[256];
        int p_idx = 0;
        for (; p_idx <= last_slash; p_idx++) {
            parent_path[p_idx] = pathname[p_idx];
        }
        if (last_slash > 0) parent_path[last_slash] = '\0';
        else parent_path[1] = '\0';

        // 分離出檔名
        int d_idx = 0;
        for (int i = last_slash + 1; pathname[i] != '\0'; i++) {
            if (d_idx < MAX_NAME - 1) {
                dirname[d_idx++] = pathname[i];
            }
        }
        dirname[d_idx] = '\0';

        // 取得父目錄的 vnode
        int ret = vfs_lookup_from(base, parent_path, &parent_vnode);
        if (ret != 0) return ret;
    }

    // 呼叫 fs api 完成 vnode 和 interal 的建立
    struct vnode* dummy = 0;
    if (!parent_vnode->v_ops || !parent_vnode->v_ops->mkdir) return -1;
    return parent_vnode->v_ops->mkdir(parent_vnode, &dummy, dirname);
}

// 基於目前的 CWD 結構更換工作目錄
int vfs_chdir(struct vnode** current_cwd, const char* pathname) {
    struct vnode* target_vnode = 0;
    int ret = vfs_lookup_from(*current_cwd, pathname, &target_vnode);
    if (ret != 0) return ret;

    if (target_vnode->type != VFS_TYPE_DIR) return -1;

    *current_cwd = target_vnode;
    return 0;
}

int vfs_mount(const char* target, const char* filesystem) {
    struct task_struct* curr = get_current();
    struct vnode* base = (curr && curr->cwd) ? curr->cwd : rootfs->root;
    
    // 找到要被 mount 的 vnode
    struct vnode* mountpoint_vnode = 0;
    int ret = vfs_lookup_from(base, target, &mountpoint_vnode);
    if (ret != 0) return ret;

    uint64_t s = disable_and_save_sstatus();
    // 從已註冊的 filesystem struct 取得對應的 structure
    struct filesystem* fs_driver = 0;
    for (int i = 0; i < registered_count; i++) {
        if (strcmp(registered_filesystems[i]->name, filesystem) == 0) {
            fs_driver = registered_filesystems[i];
            break;
        }
    }
    restore_sstatus(s);
    if (!fs_driver) return -1;

    // 建立 mount structure 來連接 mountpoint 和 fs 之 root
    struct mount* mnt = (struct mount*)allocate(sizeof(struct mount));
    if (!mnt) return -1;

    // 呼叫 fs 的 mount api 來完成 mount procedure
    ret = fs_driver->setup_mount(fs_driver, mnt);
    if (ret != 0) {
        free(mnt);
        return ret;
    }

    // 將 mountpoint node 和 mount node 互指
    mnt->mountpoint = mountpoint_vnode;
    mountpoint_vnode->mount = mnt;

    // 將 mount node 填入全域 table，方便未來查詢
    s = disable_and_save_sstatus();
    if (active_mount_count < 8) {
        active_mounts[active_mount_count++] = mnt;
    }
    restore_sstatus(s);
    return 0;
}

int vfs_close(struct file* file) {
    if (!file) return -1;
    // 釋放 file handle 結構體空間
    file->ref_count--;
    if(file->ref_count == 0) free(file);
    return 0;
}

int vfs_read(struct file* file, void* buf, size_t len) {
    if (!file || !file->f_ops || !file->f_ops->read) return -1;
    return file->f_ops->read(file, buf, len);
}

int vfs_write(struct file* file, const void* buf, size_t len) {
    if (!file || !file->f_ops || !file->f_ops->write) return -1;
    return file->f_ops->write(file, buf, len);
}

// vfs.c 尾端補上
long vfs_lseek64(struct file* file, long offset, int whence) {
    if (!file || !file->f_ops || !file->f_ops->lseek64) return -1;
    return file->f_ops->lseek64(file, offset, whence);
}

int vfs_ioctl(struct file* file, unsigned long request, void* args) {
    if (!file || !file->f_ops || !file->f_ops->ioctl) return -1;
    return file->f_ops->ioctl(file, request, args);
}