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
    fprintf(stdout, "FS info:\n");
    fprintf(stdout, "total_blk_count: %d\n", super.disk_blocks);
    fprintf(stdout, "fat_blk_count: %d\n", super.block_fat);
    fprintf(stdout, "rdir_blk: %d\n", super.root_index);
    fprintf(stdout, "data_blk: %d\n", super.dblock_index);
    fprintf(stdout, "data_blk_count: %d\n", super.num_blocks);
    fprintf(stdout, "fat_free_ratio: %d", free_fat_blocks());
    fprintf(stdout, "/%d\n", super.num_blocks);
    fprintf(stdout, "rdir_free_ratio: %d", empty_root_entries());
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
            fprintf(stdout, "file: %s, size: %d, data: %d", \
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
    UNUSED(fd);
    UNUSED(buf);
    UNUSED(count);
    return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    UNUSED(fd);
    UNUSED(buf);
    UNUSED(count);
    return 0;
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
