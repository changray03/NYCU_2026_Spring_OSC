typedef unsigned char      uint8_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;
typedef unsigned long      uintptr_t;
typedef unsigned long      size_t;

#define NULL ((void*)0)

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static const char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return s; s++; }
    return NULL;
}

static inline const void *align4(const void *p) {
    return (const void *)(((uintptr_t)p + 3) & ~3);
}

/* 安全讀取大端序 32-bit (解決 Alignment Trap) */
static uint32_t fdt32_to_cpu(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

/* 跳過目前節點及其子節點 (用於路徑匹配失敗時) */
static const uint32_t *skip_node(const uint32_t *p) {
    int level = 1;
    while (level > 0) {
        uint32_t tag = fdt32_to_cpu(p);
        p++;
        if (tag == FDT_BEGIN_NODE) {
            p = (const uint32_t *)align4((const char *)p + strlen((const char *)p) + 1);
            level++;
        } else if (tag == FDT_END_NODE) {
            level--;
        } else if (tag == FDT_PROP) {
            uint32_t len = fdt32_to_cpu(p);
            p += 2; 
            p = (const uint32_t *)align4((const char *)p + len);
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
        p = (const uint32_t *)align4((const char *)p + strlen((const char *)p) + 1);
    }

    while (1) {
        uint32_t tag = fdt32_to_cpu(p);
        const uint32_t *tag_addr = p;
        p++;

        if (tag == FDT_BEGIN_NODE) {
            const char *node_name = (const char *)p;
            p = (const uint32_t *)align4(node_name + strlen(node_name) + 1);

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
            p = (const uint32_t *)align4((const char *)p + len);
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
    p = (const uint32_t *)align4((const char *)p + strlen((const char *)p) + 1);

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
            p = (const uint32_t *)align4((const char *)p + len);
        } else if (tag == FDT_NOP) {
            continue;
        } else {
            break;
        }
    }
    return NULL;
}