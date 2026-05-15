#include "cpio.h"
#include "uart.h"
#include "stddef.h"
#include "stdint.h"
#include "utils.h"
#include "fdt.h"

void* cpio_base;

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

void* cpio_get_file(const char* filename){
    char* p = (char*)cpio_base;
    if (!p) return NULL;
    while (1) {
        struct cpio_newc_header* hdr = (struct cpio_newc_header*)p;
        if (strncmp(hdr->c_magic, "070701", 6) != 0) return NULL;
        unsigned int namesize = hextoi(hdr->c_namesize, 8);
        unsigned int filesize = hextoi(hdr->c_filesize, 8);
        char* current_filename = p + sizeof(struct cpio_newc_header);
        // 檢查是否到達 CPIO 結尾
        if (memcmp(current_filename, "TRAILER!!!", 10) == 0) {
            break;
        }

        // 比對檔名
        if (strcmp(current_filename, filename) == 0) {
            // 計算資料位址：Header + Name 之後要對齊 4 bytes
            unsigned int offset = align(sizeof(struct cpio_newc_header) + namesize, 4);
            return (void*)(p + offset);
        }

        // 移動指標到下一個檔案節點
        // Header + Name 對齊 4 bytes + Data 對齊 4 bytes
        p += align(sizeof(struct cpio_newc_header) + namesize, 4) + align(filesize, 4);
    }
    return NULL;
}

void cpio_init(uint64_t fdt){
    // find cpio
    int chosen_off = fdt_path_offset((void *)fdt, "/chosen");
    if (chosen_off >= 0) {
        int len;
        const uint32_t *start_ptr = fdt_getprop((void *)fdt, chosen_off, "linux,initrd-start", &len);
        if (start_ptr) {
            if (len == 4) {
                cpio_base = (void *)(uintptr_t)fdt32_to_cpu(start_ptr);
            } else {
                cpio_base = (void *)(((uint64_t)fdt32_to_cpu(start_ptr) << 32) | fdt32_to_cpu(start_ptr + 1));
            }
        }
    }
    if (cpio_base) {
        uart_puts("Initrd found at: ");
        uart_hex((unsigned long)cpio_base);
        uart_puts("\n");
    } else {
        uart_puts("Error: Could not find initrd in devicetree.\n");
    }
}