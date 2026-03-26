typedef unsigned char      uint8_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;
typedef unsigned long      size_t;

extern void uart_puts(const char* s);
extern void uart_putc(char c);
extern void uart_decimal(unsigned long d);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern size_t strlen(const char *s);

struct cpio_newc_header {
    char c_magic[6];     
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];  
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];  
    char c_check[8];
};

static uint32_t hex8_to_uint(char *s) {
    uint32_t res = 0;
    for (int i = 0; i < 8; i++) {
        res <<= 4;
        if (s[i] >= '0' && s[i] <= '9') res += s[i] - '0';
        else if (s[i] >= 'A' && s[i] <= 'F') res += s[i] - 'A' + 10;
        else if (s[i] >= 'a' && s[i] <= 'f') res += s[i] - 'a' + 10;
    }
    return res;
}

void cpio_ls(void *archive) {
    char *ptr = (char *)archive;

    while (1) {
        struct cpio_newc_header *header = (struct cpio_newc_header *)ptr;

        // 檢查 Magic Number 確保格式正確
        if (strncmp(header->c_magic, "070701", 6) != 0) break;

        uint32_t namesize = hex8_to_uint(header->c_namesize);
        uint32_t filesize = hex8_to_uint(header->c_filesize);
        char *filename = ptr + sizeof(struct cpio_newc_header);

        // 如果檔名是 "TRAILER!!!" 代表封存檔結束 
        if (strcmp(filename, "TRAILER!!!") == 0) break;

        // 印出檔名
        uart_decimal(filesize);
        uart_puts(" ");
        uart_puts(filename);
        uart_puts("\n");

        uint32_t offset = sizeof(struct cpio_newc_header) + namesize;
        offset = (offset + 3) & ~3; // align to 4 bytes
        offset += filesize;
        offset = (offset + 3) & ~3; 

        ptr += offset;
    }
}

void cpio_cat(void *archive, const char *target_name) {
    char *ptr = (char *)archive;

    while (1) {
        struct cpio_newc_header *header = (struct cpio_newc_header *)ptr;
        if (strncmp(header->c_magic, "070701", 6) != 0) break;

        uint32_t namesize = hex8_to_uint(header->c_namesize);
        uint32_t filesize = hex8_to_uint(header->c_filesize);
        char *filename = ptr + sizeof(struct cpio_newc_header);

        if (strcmp(filename, "TRAILER!!!") == 0) break;

        if (strcmp(filename, target_name) == 0) {
            // 找到檔案，計算資料位址
            uint32_t offset = sizeof(struct cpio_newc_header) + namesize;
            offset = (offset + 3) & ~3; // 檔名對齊 
            char *data = ptr + offset;

            // 印出內容 (依照 filesize)
            for (uint32_t i = 0; i < filesize; i++) {
                uart_putc(data[i]);
            }
            uart_puts("\n");
            return;
        }

        uint32_t offset = sizeof(struct cpio_newc_header) + namesize;
        offset = (offset + 3) & ~3;
        offset += filesize;
        offset = (offset + 3) & ~3;
        ptr += offset;
    }
    uart_puts("File not found!\n");
}