#ifndef CPIO_H
#define CPIO_H

#include <stdint.h>
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
extern void* cpio_base;

void cpio_ls(void *archive);
void cpio_cat(void *archive, const char *target_name);
void cpio_init(uint64_t fdt);
void* cpio_get_file(const char* filename, uint64_t *size);
void* cpio_get_all_files(void* current_ptr, const char** name_ptr, uint64_t* size_ptr, void** next_ptr);
#endif