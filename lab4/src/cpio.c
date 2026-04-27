#include "cpio.h"
#include "uart.h"
#include "stddef.h"
#include "stdint.h"
#include "utils.h"

void cpio_ls(void *archive) {
    char *ptr = (char *)archive;

    while (1) {
        struct cpio_newc_header *header = (struct cpio_newc_header *)ptr;

        // 檢查 Magic Number 確保格式正確
        if (strncmp(header->c_magic, "070701", 6) != 0) break;

        uint32_t namesize = hextoi(header->c_namesize, 8);
        uint32_t filesize = hextoi(header->c_filesize, 8);
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

        uint32_t namesize = hextoi(header->c_namesize, 8);
        uint32_t filesize = hextoi(header->c_filesize, 8);
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