#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define FBLOCK_SIZE 2048
/// To be removed
#define UNUSED(x) (void)(x)

/* TODO: Phase 1 */
struct __attribute__ ((packed)) super_block {
    uint8_t signature[8];
    uint16_t disk_blocks;
    uint16_t root_index;
    uint16_t dblock_index;
    uint16_t num_blocks;
    uint8_t block_fat;
    uint8_t padding[4079];
};

struct __attribute__ ((packed)) FAT {
    uint16_t* fat_data;
};

typedef struct FAT* fat_t;

struct __attribute__ ((packed)) root_entry {
    char filename[FS_FILENAME_LEN];
    uint32_t file_size;
    uint16_t block1_index;
    uint8_t padding[10];
};

struct __attribute__ ((packed)) fd {
    int offset;
    uint8_t file[FS_FILENAME_LEN];
    uint32_t size;
    uint16_t index;
    int is_open;
};

typedef struct root_entry* root_t;

int empty_root_entries(void );
int find_file(const char* filename);
int find_first_empty(void);
int first_fit(void);
int first_open_fd(void );
int free_fat_blocks(void );

struct root_entry root_directory[FS_FILE_MAX_COUNT];
struct fd file_descriptor[FS_FILE_MAX_COUNT];
struct FAT fat_block;
struct super_block super;
unsigned is_mounted = 0;
int num_open_files = 0;

int fs_mount(const char *diskname) {
    if (!diskname) {
        return -1;
    }
    if (block_disk_open(diskname) == -1) {
        return -1;
    }
    is_mounted = 1;
    size_t block_num = 0;
    if (block_read(block_num, &super) != 0) {
        return -1;
    }
    if (block_disk_count() != super.disk_blocks) {
        return -1;
    }
    if (!strcmp((char*)super.signature, "ECS150FS")) {
        return -1;
    }
    block_num++;
    fat_block.fat_data = (uint16_t* ) malloc(sizeof(uint16_t) * BLOCK_SIZE
            * super.block_fat);
    for (block_num = 1; block_num <= super.block_fat; block_num++) {
        if (block_read(block_num, &fat_block.fat_data[(block_num - 1)\
        *FBLOCK_SIZE]) != 0) {
            return -1;
        }
    }
    block_num = super.block_fat + 1;
    fat_block.fat_data[0] = FAT_EOC;
    if (block_read(block_num, &root_directory) != 0) {
        return -1;
    }
    return 0;
}

int fs_umount(void) {
	/* TODO: Phase 1 */
    if (is_mounted != 1) {
        return -1;
    }
    is_mounted = 0;
    int block_num = 0;
    if (block_write(block_num, &super) != 0) {
        return -1;
    }
    block_num++;
    for (block_num = 1; block_num <= super.block_fat; block_num++) {
        block_write(block_num, &fat_block.fat_data[(block_num - 1) \
        *FBLOCK_SIZE]);
    }
    free(fat_block.fat_data);
    block_num = super.block_fat + 1;
    if (block_write(block_num, &root_directory) != 0) {
        return -1;
    }
    if (block_disk_close() == -1) {
        return -1;
    }
    return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
    if (is_mounted != 1) {
        return -1;
    }
    fprintf(stdout, "FS Info:\n");
    fprintf(stdout, "total_blk_count=%d\n", super.disk_blocks);
    fprintf(stdout, "fat_blk_count=%d\n", super.block_fat);
    fprintf(stdout, "rdir_blk=%d\n", super.root_index);
    fprintf(stdout, "data_blk=%d\n", super.dblock_index);
    fprintf(stdout, "data_blk_count=%d\n", super.num_blocks);
    fprintf(stdout, "fat_free_ratio=%d", free_fat_blocks());
    fprintf(stdout, "/%d\n", super.num_blocks);
    fprintf(stdout, "rdir_free_ratio=%d", empty_root_entries());
    fprintf(stdout, "/%d\n", FS_FILE_MAX_COUNT);
    return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
    if (!filename) {
        return -1;
    }
    if (is_mounted == 0) {
        return -1;
    }
    if (strlen(filename) > FS_FILENAME_LEN) {
        return -1;
    }
    int empty_entry = find_first_empty();
    if (empty_entry == -1) {
        return -1;
    }
    strcpy(root_directory[empty_entry].filename, filename);
    root_directory[empty_entry].file_size = 0;
    root_directory[empty_entry].block1_index = FAT_EOC;
    return 0;
}

int fs_delete(const char *filename) {
	/* TODO: Phase 2 */
    if (is_mounted == 0) {
        return -1;
    }
    if (!filename) {
        return -1;
    }
    int file_index = find_file(filename);
    int fat_index = 0;
    if (file_index == -1) {
        return -1;
    }
    memset(root_directory[file_index].filename, '\0', FS_FILENAME_LEN);
    root_directory[file_index].file_size = 0;
    fat_index = root_directory[file_index].block1_index;
    root_directory[file_index].block1_index = 0;

    while (fat_index != FAT_EOC) {
        int next_value = fat_block.fat_data[fat_index];
        fat_block.fat_data[fat_index] = 0;
        fat_index = next_value;
    }
    return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
    if (is_mounted == 0) {
        return -1;
    }
    fprintf(stdout, "FS Ls:\n");
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].block1_index != 0) {
            fprintf(stdout, "file: %s, size: %d, data_blk: %d", \
                    root_directory[i].filename, root_directory[i].file_size,
                    root_directory[i].block1_index);
            printf("\n");
        }
    }
    return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
    if (!filename) {
        return -1;
    }
    if (is_mounted == 0) {
        return -1;
    }
    int descriptor = first_open_fd();
    if (descriptor == -1) {
        return -1;
    }
    int file_match = find_file(filename);
    if (file_match == -1) {
        return -1;
    }
    file_descriptor[descriptor].is_open = 1;
    file_descriptor[descriptor].offset = 0;
    strcpy((char*)file_descriptor[descriptor].file, \
           (char*)root_directory[file_match].filename);
    file_descriptor[descriptor].index =
            root_directory[file_match].block1_index;
    file_descriptor[descriptor].size = root_directory[file_match].file_size;
    return 0;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
    if (is_mounted == 0) {
        return -1;
    }
    if (fd < 0) {
        return -1;
    }
    if (file_descriptor[fd].is_open != 1) {
        return -1;
    }
    file_descriptor[fd].is_open = 0;
    memset(file_descriptor[fd].file, '\0', FS_FILENAME_LEN);
    file_descriptor[fd].index = 0;
    file_descriptor[fd].size = 0;
    file_descriptor[fd].offset = 0;
    return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
    if (is_mounted == 0) {
        return -1;
    }
    if (fd < 0) {
        return -1;
    }
    if (file_descriptor[fd].is_open != 1) {
        return -1;
    }
    int result = file_descriptor[fd].size;
    return result;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
    if (is_mounted == 0) {
        return -1;
    }
    if (fd < 0) {
        return -1;
    }
    if (file_descriptor[fd].is_open != 1) {
        return -1;
    }
    if (file_descriptor[fd].size < offset) {
        return -1;
    }
    file_descriptor[fd].offset = offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    char cur_filename[FS_FILENAME_LEN];
    int cur_off;
    int read_index = 1;
    int start_index;
    int fat_new;
    int root_new;
    int starting_block;
    uint8_t * bounce_buf = (uint8_t * ) malloc(sizeof(uint8_t) * BLOCK_SIZE);
    uint8_t * user_supplied_buf = (uint8_t* ) malloc(sizeof(uint8_t) * count);
    memcpy(&user_supplied_buf[0], buf, count);
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (file_descriptor[fd].is_open) {
            cur_off = file_descriptor[i].offset;
            strcpy(cur_filename, (char *)file_descriptor[i].file);
            break;
        }
    }
    while((cur_off - 4096) > 0){
        cur_off -= 4096;
        read_index += 1;
    }
    start_index = find_file(cur_filename);
    root_directory[start_index].file_size += (int)count;
    root_new = root_directory[start_index].block1_index;
    fat_new = root_new;
    for (int i = 1; i < read_index; i++){
        fat_new = fat_block.fat_data[fat_new];
    }
    if(root_new == FAT_EOC){
        for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
            if (strcmp((char *)root_directory[i].filename, cur_filename) == 0) {
                root_directory[start_index].block1_index = fat_new;
                break;
            }
        }     
    }
    if(fat_new == FAT_EOC){
        for (int i = 1; i < super.block_fat; i++) {
            if (fat_block.fat_data[i] == 0) {
                fat_new = i;
                fat_block.fat_data[i] = FAT_EOC;
                break;
            }
        }
    }
    starting_block = super.dblock_index + fat_new;
    size_t fin_bytes = 0;
    size_t rem_bytes = count;
    size_t cur_bytes = 0;
    while(fin_bytes < count){
        size_t bytes_new = 4096 - (cur_off % 4096);
        block_read(starting_block, (void *)bounce_buf);
        if(rem_bytes <= bytes_new){
            cur_bytes = rem_bytes;
        } 
        else{
            cur_bytes = bytes_new;
        }
        memcpy(&bounce_buf[cur_off % 4096], &user_supplied_buf[fin_bytes], cur_bytes);
        block_write(starting_block, (void *)bounce_buf);
        rem_bytes = rem_bytes - cur_bytes;
        fin_bytes += cur_bytes;
        cur_off = cur_off + cur_bytes;
        if(fin_bytes < count){
            if(fat_block.fat_data[fat_new] == FAT_EOC){
                for (int i = 1; i < super.block_fat; i++) {
                    if (fat_block.fat_data[i] == 0) {
                        fat_block.fat_data[fat_new] = i;
                        fat_block.fat_data[i] = FAT_EOC;
                        break;
                    }
                }
            }

        }
        fat_new = fat_block.fat_data[fat_new];
        starting_block = super.dblock_index + fat_new;
    }
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (file_descriptor[fd].is_open) {
            file_descriptor[i].offset = cur_off;
            break;
        }
    }
    return fin_bytes;  
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    char cur_filename[FS_FILENAME_LEN];
    int cur_off;
    int read_index = 1;
    int start_index;
    int fat_new;
    int starting_block;
    uint8_t * bounce_buf = (uint8_t * ) malloc(sizeof(uint8_t) * BLOCK_SIZE);
    uint8_t * user_supplied_buf = (uint8_t* ) malloc(sizeof(uint8_t) * count);
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (file_descriptor[fd].is_open) {
            cur_off = file_descriptor[i].offset;
            strcpy(cur_filename, (char *)file_descriptor[i].file);
            break;
        }
    }
    while((cur_off - 4096) > 0){
        cur_off -= 4096;
        read_index += 1;
    }
    start_index = find_file(cur_filename);
    fat_new = root_directory[start_index].block1_index;
    for (int j = 1; j < read_index; j++){
        fat_new = fat_block.fat_data[fat_new];
    }
    starting_block = super.dblock_index + fat_new;
    size_t fin_bytes = 0;
    size_t rem_bytes = count;
    size_t cur_bytes = 0;
    while(fin_bytes < count){
        size_t bytes_new = 4096 - (cur_off % 4096);
        block_read(starting_block, (void *)bounce_buf);
        if(rem_bytes <= bytes_new){
            cur_bytes = rem_bytes;
        } 
        else{
            cur_bytes = bytes_new;
        }
        memcpy(&user_supplied_buf[fin_bytes], &bounce_buf[cur_off % 4096], cur_bytes);
        rem_bytes = rem_bytes - cur_bytes;
        fin_bytes += cur_bytes;
        cur_off = cur_off + cur_bytes;
        fat_new = fat_block.fat_data[fat_new];
        starting_block = super.dblock_index + fat_new;
    }
    memcpy(buf, &user_supplied_buf[0], count);
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (file_descriptor[fd].is_open) {
            file_descriptor[i].offset = cur_off;
            break;
        }
    }
    return fin_bytes;

}

int empty_root_entries() {
    int result = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename[0] == '\0') {
            result++;
        }
    }
    return result;
}

int find_file(const char* filename) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strcmp((char *)root_directory[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

int find_first_empty(void) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename[0] == '\0') {
            return i;
        }
    }
    return -1;
}

int first_fit() {
    for (int i = 0; i < super.block_fat; i++) {
        if (fat_block.fat_data[i * BLOCK_SIZE] == 0) {
            return i;
        }
    }
    return -1;
}

int first_open_fd() {
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (file_descriptor[i].file[0] == '\0') {
            return i;
        }
    }
    return -1;
}

int free_fat_blocks() {
    int result = 0;
    for (int i = 0; i < super.num_blocks; i++) {
        if (fat_block.fat_data[i] == 0) {
            result++;
        }
    }
    return result;
}
