#include "fdt.h"
#include "utils.h"

/* 跳過目前節點及其子節點 (用於路徑匹配失敗時) */
const uint32_t *skip_node(const uint32_t *p) {
    int level = 1;
    while (level > 0) {
        uint32_t tag = fdt32_to_cpu(p);
        p++;
        if (tag == FDT_BEGIN_NODE) {
            p = (const uint32_t *)align((uint64_t)p + strlen((const char *)p) + 1, 4);
            level++;
        } else if (tag == FDT_END_NODE) {
            level--;
        } else if (tag == FDT_PROP) {
            uint32_t len = fdt32_to_cpu(p);
            p += 2; 
            p = (const uint32_t *)align((uint64_t)p + len, 4);
        } else if (tag == FDT_END) {
            return NULL;
        }
    }
    return p;
}

int fdt_path_offset(const void *fdt, const char *path) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    if (fdt32_to_cpu(&h->magic) != 0xd00dfeed) return -1;

    const char *struct_base = (const char *)fdt + fdt32_to_cpu(&h->off_dt_struct);
    const uint32_t *p = (const uint32_t *)struct_base;

    if (*path != '/') return -1;
    if (path[1] == '\0') return 0;

    const char *cur_seg = path + 1;
    
    if (fdt32_to_cpu(p) == FDT_BEGIN_NODE) {
        p++; 
        p = (const uint32_t *)align((uint64_t)p + strlen((const char *)p) + 1, 4);
    }

    while (1) {
        uint32_t tag = fdt32_to_cpu(p);
        const uint32_t *tag_addr = p;
        p++;

        if (tag == FDT_BEGIN_NODE) {
            const char *node_name = (const char *)p;
            p = (const uint32_t *)align((uint64_t)node_name + strlen(node_name) + 1, 4);

            const char *slash = strchr(cur_seg, '/');
            size_t seg_len = slash ? (size_t)(slash - cur_seg) : strlen(cur_seg);

            if (strncmp(node_name, cur_seg, seg_len) == 0 && 
               (node_name[seg_len] == '\0' || node_name[seg_len] == '@')) {
                
                if (slash == NULL || *(slash + 1) == '\0') {
                    return (int)((const char *)tag_addr - struct_base);
                }
                cur_seg = slash + 1;
                continue; 
            }
            p = skip_node(p); 
            if (!p) return -1;
        } else if (tag == FDT_PROP) {
            uint32_t len = fdt32_to_cpu(p);
            p += 2;
            p = (const uint32_t *)align((uint64_t)p + len, 4);
        } else if (tag == FDT_NOP) {
            continue;
        } 
        else {
            return -1;
        }
    }
}

const void *fdt_getprop(const void *fdt, int nodeoffset, const char *name, int *lenp) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    const char *struct_base = (const char *)fdt + fdt32_to_cpu(&h->off_dt_struct);
    const char *strings_base = (const char *)fdt + fdt32_to_cpu(&h->off_dt_strings);
    const uint32_t *p = (const uint32_t *)(struct_base + nodeoffset);
    
    if (fdt32_to_cpu(p) != FDT_BEGIN_NODE) return NULL;
    p++;
    p = (const uint32_t *)align((uint64_t)p + strlen((const char *)p) + 1, 4);

    while (1) {
        uint32_t tag = fdt32_to_cpu(p);
        p++;

        if (tag == FDT_PROP) {
            uint32_t len = fdt32_to_cpu(p);
            uint32_t nameoff = fdt32_to_cpu(p + 1);
            p += 2;
            if (strcmp(strings_base + nameoff, name) == 0) {
                if (lenp) *lenp = (int)len;
                return p;
            }
            p = (const uint32_t *)align((uint64_t)p + len, 4);
        } else if (tag == FDT_NOP) {
            continue;
        } else {
            break;
        }
    }
    return NULL;
}