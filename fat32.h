#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>

int fat32_init(void);
void fat32_list_directory(void (*print_fn)(const char*));
bool fat32_read_file(const char* filename, char* out_buf, uint32_t max_len, uint32_t* out_size);

#endif
