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
struct super_block {
    uint8_t[8] signature;
    uint16_t disk_blocks;
    uint16_t root_index;
    uint16_t dblock_index;
    uint16_t num_blocks;
    uint8_t block_fat;
    uint8_t[4079] padding;
} __attribute__ ((packed));

typedef struct super_block* super_t;

struct FAT {
    uint16_t* fat_data;
} __attribute__ ((packed));

typedef struct FAT* fat_t;

struct root_entry {
    uint8_t[FS_FILENAME_LEN] filename;
    uint32_t file_size;
    uint16_t block1_index;
    uint8_t[10] padding;
} __attribute__ ((packed));

int empty_root_entries(struct root_entry* root_directory);
int find_file(struct root_entry* root_directory, const char* filename);
int find_first_empty(struct root_entry* root_directory);

struct root_entry[FS_FILE_MAX_COUNT] root_directory;
fat_t fat_blocks;
super_t super;
unsigned is_mounted;

int fs_mount(const char *diskname) {
    if (!diskname) {
        return -1;
    }
    if (block_disk_open(diskname) == -1) {
        return -1;
    }

    if (block_disk_count() != super -> disk_blocks) {
        return -1;
    }

    is_mounted = 1;

    size_t block_num = 0;

    super = (super_t) malloc(sizeof(struct super_block));
    if (super == NULL) {
        return -1;
    }
    if (block_read(block_num, super) != 0) {
        return -1;
    }
    if (memcmp(super -> signature, "ECS150FS", sizeof(super -> signature))!=0) {
        return -1;
    }

    fat_blocks = (fat_t) malloc(sizeof(struct FAT));
    if (fat_blocks == NULL) {
        return -1;
    }
    fat_blocks -> fat_data = (uint16_t* ) malloc(sizeof(uint16_t) * FBLOCK_SIZE
            * super -> fat_blocks);
    if (fat_blocks == NULL) {
        return -1;
    }
    for (block_num = 1; block_num <= super -> block_fat; block_num++) {
        if (block_read(block_num, &fat_blocks -> fat_data[(block_num - 1)\
        *BLOCK_SIZE]) != 0) {
            return -1;
        }
    }

    if (block_read(block_num, root_directory) != 0) {
        return -1;
    }
    return 0;

}

int fs_umount(void) {
	/* TODO: Phase 1 */
    if (is_mounted != 1) {
        return -1;
    }
    if (block_disk_close() == -1) {
        return -1;
    }
    is_mounted = 0;
    int block_num = 0;
    if (block_write(block_num, super) != 0) {
        return -1;
    }
    block_num++;
    free(super);
    for (block_num = 1; block_num <= super -> block_fat; block_num++) {
        if (block_write(block_num, &fat_blocks -> fat_data[(block_num - 1)\
        *BLOCK_SIZE]) != 0) {
            return -1;
        }
    }
    free(fat_blocks -> fat_data);
    free(fat_blocks);
    if (block_write(block_num, root_directory) != 0) {
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
    fprintf(stderr, "FS info:\n");
    fprintf(stderr, "total_blk_count: %d\n", super -> disk_blocks);
    fprintf(stderr, "fat_blk_count: %d\n", super -> block_fat);
    fprintf(stderr, "rdir_blk: %d\n", super -> root_index);
    fprintf(stderr, "data_blk: %d\n", super -> dblock_index);
    fprintf(stderr, "data_blk_count: %d\n", super -> disk_blocks);
    fprintf(stderr, "rdir_free_ratio: %d/&d \n",
            empty_root_entries(root_directory), FS_FILE_MAX_COUNT);
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
    if (!filename) {
        return -1;
    }
    if (is_mounted == 0) {
        return -1
    }
    if (strlen(filename) > FS_FILENAME_LEN) {
        return -1;
    }
    size_t empty_entry = find_first_empty(root_directory);
    if (empty_entry == -1) {
        return -1;
    }
    root_directory[empty_entry].filename = filename;
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
    int file_index = find_file(root_directory, filename);
    int fat_index;
    if (file_index == -1) {
        return -1;
    }
    root_directory[file_index].filename = NULL;
    root_directory[file_index].file_size = 0;
    fat_index = root_directory[file_index].block1_index;
    root_directory[file_index].block1_index = FAT_EOC;

    while (fat_blocks->fat_data[fat_index] != FAT_EOC) {
        uint16_t next_value = fat_blocks -> fat_data[fat_index];
        fat_blocks -> fat_data[fat_index] = 0;
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
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename != NULL) {
            fprintf(stderr, "%d\n", root_directory[i].filename);
        }
    }
    return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int empty_root_entries(struct root_entry* root_directory) {
    int result = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename != NULL) {
            result++;
        }
    }
    return result;
}

int find_file(struct root_entry* root_directory, const char* filename) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename == (uint8_t*)filename) {
            return i;
        }
    }
    return -1;
}

int find_first_empty(struct root_entry* root_directory) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename == NULL) {
            return i;
        }
    }
    return -1;
}
