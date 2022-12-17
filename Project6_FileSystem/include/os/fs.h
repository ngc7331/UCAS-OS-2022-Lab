#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0x20221205
#define NUM_FDESCS 16
#define FS_START 0x20000
#define SECTOR_SIZE 512
#define BLOCK_SIZE_BYTE 0x1000 // 4KB
#define BLOCK_SIZE (BLOCK_SIZE_BYTE / SECTOR_SIZE)
#define MAX_BLOCK_NUM 0x100000 // 4GB = 0x100000 blocks
#define MAX_INODE_NUM 0x8000
#define MAX_BLOCK_MAP_SIZE (MAX_BLOCK_NUM / BLOCK_SIZE_BYTE / 8)
#define MAX_INODE_MAP_SIZE (MAX_INODE_NUM / BLOCK_SIZE_BYTE / 8)

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

/* data structures of file system */
typedef struct superblock_t {
    // NOTE: start / offset / size's unit is block
    int magic0;
    uint32_t fs_start;
    uint32_t inode_map_offset;
    uint32_t inode_map_size;
    int inode_num;
    uint32_t block_map_offset;
    uint32_t block_map_size;
    int block_num;
    uint32_t inode_offset;
    uint32_t data_offset;
    int magic1;
} superblock_t;

typedef struct dentry_t {
    // NOTE: BLOCK_SIZE_BYTE must be divisible by sizeof(this)
    char name[56];
    int ino;
    int valid;
} dentry_t;

#define DIRECT_BLOCK_NUM 8
#define INDIRECT_BLOCK_L1_NUM 3
#define INDIRECT_BLOCK_L2_NUM 2
#define INDIRECT_BLOCK_L3_NUM 1

#define INODE_FILE 0
#define INODE_DIR  1
typedef struct inode_t {
    // NOTE: BLOCK_SIZE_BYTE must be divisible by sizeof(this)
    short type;
    short link;
    int size;
    int direct_blocks[DIRECT_BLOCK_NUM];
    int indirect_blocks_l1[INDIRECT_BLOCK_L1_NUM];
    int indirect_blocks_l2[INDIRECT_BLOCK_L2_NUM];
    int indirect_blocks_l3[INDIRECT_BLOCK_L3_NUM];
} inode_t;

typedef struct fdesc_t {
    int ino;
    int wp;
    int rp;
    int mode;
    pid_t owner;
} fdesc_t;

/* modes of do_fopen */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
void init_fs(void);
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

#endif