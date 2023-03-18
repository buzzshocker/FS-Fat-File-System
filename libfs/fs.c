#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define FBLOCK_SIZE 2048

/* TODO: Phase 1 */
struct __attribute__ ((packed)) super_block {
    uint8_t signature[8];   // Signature of the file - Always "ECS150FS"
    uint16_t disk_blocks;   // Total number of virtual disk blocks
    uint16_t root_index;    // Block index of root directory
    uint16_t dblock_index;  // First data block index
    uint16_t num_blocks;    // Number of data blocks
    uint8_t block_fat;      // Number of fat blocks
    uint8_t padding[4079];
};

struct __attribute__ ((packed)) FAT {
    uint16_t* fat_data;  // Array containing the fat blocks
};

typedef struct FAT* fat_t;

struct __attribute__ ((packed)) root_entry {
    char filename[FS_FILENAME_LEN];  // Filename
    uint32_t file_size;              // Size of file
    uint16_t block1_index;           // Index of first data block
    uint8_t padding[10];
};

struct __attribute__ ((packed)) fd {
    int offset;                         // File offset
    uint8_t file[FS_FILENAME_LEN];      // Name of the file
    uint16_t index;                     // Index of first data block
    int is_open;                        // Indicator for file being open
};

typedef struct root_entry* root_t;

// Helper functions
int empty_root_entries(void );
int find_file(const char* filename);
int find_first_empty(void);
int first_fit(void);
int first_open_fd(void );
int free_fat_blocks(void );

// Global variables
struct root_entry root_directory[FS_FILE_MAX_COUNT];
struct fd file_descriptor[FS_FILE_MAX_COUNT];
struct FAT fat_block;
struct super_block super;
unsigned is_mounted = 0;
int num_open_files = 0;


// To mount the given diskname by reading in all the blocks from that disk onto
// the appropriate global variables.
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
    // Allocating space for num fat blocks, with each index = BLOCK_SIZE
    fat_block.fat_data = (uint16_t* ) malloc(sizeof(uint16_t) * FBLOCK_SIZE
                                             * super.block_fat);
    for (block_num = 1; block_num <= super.block_fat; block_num++) {
        if (block_read(block_num, &fat_block.fat_data[(block_num - 1)\
        *FBLOCK_SIZE]) != 0) {
            return -1;
        }
    }
    block_num = super.block_fat + 1;
    if (fat_block.fat_data[0] != FAT_EOC) {
        return -1;
    }
    if (block_read(block_num, &root_directory) != 0) {
        return -1;
    }
    return 0;
}


// To unmount the currently mounted disk and write back the data blocks from the
// appropriate global variables back
int fs_umount(void) {

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

// To return important and vital information about the currently mounted disk
int fs_info(void)
{
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


// To create a new file in the currently mounted disk
int fs_create(const char *filename)
{
    if (!filename) {
        return -1;
    }
    if (is_mounted == 0) {
        return -1;
    }
    if (strlen(filename) > FS_FILENAME_LEN) {
        return -1;
    }
    if (find_file(filename) != -1) {
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


// To delete a file in the currently mounted disk and deallocating its fat block
int fs_delete(const char *filename) {

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


// To give name, space and block information about files in the disk
int fs_ls(void)
{
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


// To open the given file if present in the disk and assign it an fd
int fs_open(const char *filename)
{
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
    return 0;
}


// To close the file indicated by the fd and reset the associated variables
int fs_close(int fd)
{
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
    file_descriptor[fd].offset = 0;
    return 0;
}


// To get size information about the file pointed at by the fd
int fs_stat(int fd)
{
    if (is_mounted == 0) {
        return -1;
    }
    if (fd < 0) {
        return -1;
    }
    if (file_descriptor[fd].is_open != 1) {
        return -1;
    }
    int fd_index = find_file((char *)file_descriptor[fd].file);
    int result = root_directory[fd_index].file_size;
    return result;
}


// To change the offset of the file indicated by the given fd to the given
// offset
int fs_lseek(int fd, size_t offset)
{
    if (is_mounted == 0) {
        printf("mouned is 0\n");
        return -1;
    }
    if (fd < 0) {
        printf("fd less than 0\n");
        return -1;
    }
    if (file_descriptor[fd].is_open != 1) {
        printf("is open not 1\n");
        return -1;
    }
    int fd_index = find_file((char *)file_descriptor[fd].file);
    if (root_directory[fd_index].file_size < offset) {
        return -1;
    }
    file_descriptor[fd].offset = offset;
    return 0;
}
// To write the given bytes of data from a buffer pointer into the file
// referenced by the file descriptor
int fs_write(int fd, void *buf, size_t count)
{
    if (is_mounted == 0) {
        return -1;
    }
    if (fd < 0) {
        return -1;
    }
    if (file_descriptor[fd].is_open != 1) {
        return -1;
    }
    if (!buf) {
        return -1;
    }
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
    while((cur_off - BLOCK_SIZE) > 0){
        cur_off -= BLOCK_SIZE;
        read_index += 1;
    }
    start_index = find_file(cur_filename);
    if (start_index == -1) {
        return -1;
    }
    root_new = root_directory[start_index].block1_index;
    fat_new = root_new;
    for (int i = 1; i < read_index; i++){
        fat_new = fat_block.fat_data[fat_new];
    }
    if ((fat_new == FAT_EOC) && (count > 0)) {
        for (int i = 1; i < super.num_blocks; i++) {
            if (fat_block.fat_data[i] == 0) {
                fat_new = i;
                fat_block.fat_data[i] = FAT_EOC;
                break;
            }
        }
    }
    if((root_new == FAT_EOC) && (count > 0)){
        for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
            if (strcmp((char *)root_directory[i].filename, cur_filename) == 0) {
                root_directory[start_index].block1_index = fat_new;
                break;
            }
        }
    }
    starting_block = super.dblock_index + fat_new;
    size_t fin_bytes = 0;
    size_t rem_bytes = count;
    size_t cur_bytes = 0;
    while(fin_bytes < count){
        size_t bytes_new = BLOCK_SIZE - (cur_off % BLOCK_SIZE);
        if (block_read(starting_block, (void *)bounce_buf) != 0) {
            return -1;
        }
        if(rem_bytes <= bytes_new){
            cur_bytes = rem_bytes;
        }
        else{
            cur_bytes = bytes_new;
        }
        memcpy(&bounce_buf[cur_off % BLOCK_SIZE], &user_supplied_buf[fin_bytes],
               cur_bytes);
        if (block_write(starting_block, (void *)bounce_buf) != 0) {
            return -1;
        }
        rem_bytes = rem_bytes - cur_bytes;
        fin_bytes += cur_bytes;
        cur_off = cur_off + cur_bytes;
        if(fin_bytes < count){
            if(fat_block.fat_data[fat_new] == FAT_EOC){
                for (int i = 1; i < super.num_blocks; i++) {
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
    int the_index = find_file((char *)file_descriptor[fd].file);
    if ((int)root_directory[the_index].file_size < cur_off) {
        root_directory[the_index].file_size = cur_off;
    }
    return fin_bytes;
}
// To read the given bytes of data from a buffer pointer into the file
// referenced by the file descriptor
int fs_read(int fd, void *buf, size_t count)
{
    if (is_mounted == 0) {
        return -1;
    }
    if (fd < 0) {
        return -1;
    }
    if (file_descriptor[fd].is_open != 1) {
        return -1;
    }
    if (!buf) {
        return -1;
    }
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
    while((cur_off - BLOCK_SIZE) > 0){
        cur_off -= BLOCK_SIZE;
        read_index += 1;
    }
    start_index = find_file(cur_filename);
    if (start_index == -1) {
        return -1;
    }
    fat_new = root_directory[start_index].block1_index;
    for (int j = 1; j < read_index; j++){
        fat_new = fat_block.fat_data[fat_new];
    }
    starting_block = super.dblock_index + fat_new;
    size_t fin_bytes = 0;
    size_t rem_bytes = count;
    size_t cur_bytes = 0;
    while(fin_bytes < count){
        size_t bytes_new = BLOCK_SIZE - (cur_off % BLOCK_SIZE);
        if (block_read(starting_block, (void *)bounce_buf)) {
            return -1;
        }
        if(rem_bytes <= bytes_new){
            cur_bytes = rem_bytes;
        }
        else{
            cur_bytes = bytes_new;
        }
        memcpy(&user_supplied_buf[fin_bytes], &bounce_buf[cur_off % BLOCK_SIZE],
               cur_bytes);
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


/// Helper functions

// Find the number of empty root entries
int empty_root_entries() {
    int result = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename[0] == '\0') {
            result++;
        }
    }
    return result;
}


// Find the index of the given filename in the root directory
int find_file(const char* filename) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strcmp((char *)root_directory[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}


// Find first empty root directory entry
int find_first_empty(void) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename[0] == '\0') {
            return i;
        }
    }
    return -1;
}


// Find first empty fat block entry
int first_fit() {
    for (int i = 0; i < super.block_fat; i++) {
        if (fat_block.fat_data[i * BLOCK_SIZE] == 0) {
            return i;
        }
    }
    return -1;
}


// Find the first open file descriptor
int first_open_fd() {
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (file_descriptor[i].file[0] == '\0') {
            return i;
        }
    }
    return -1;
}


// Find the number of free fat blocks
int free_fat_blocks() {
    int result = 0;
    for (int i = 0; i < super.num_blocks; i++) {
        if (fat_block.fat_data[i] == 0) {
            result++;
        }
    }
    return result;
}
