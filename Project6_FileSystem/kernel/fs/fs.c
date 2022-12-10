#include <os/kernel.h>
#include <os/loader.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/fs.h>
#include <printk.h>

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];

static uint8_t buf[BLOCK_SIZE_BYTE];

static int is_fs_avaliable(void) {
    return superblock.magic0 == SUPERBLOCK_MAGIC && superblock.magic1 == SUPERBLOCK_MAGIC;
}

static int write_buf(uint32_t offset) {
    return bios_sdwrite(kva2pa((uint64_t) buf), BLOCK_SIZE, FS_START + offset);
}

static int read_buf(uint32_t offset) {
    return bios_sdread(kva2pa((uint64_t) buf), BLOCK_SIZE, FS_START + offset);
}

void init_fs(void) {
    // NOTE: this should be called only by kernel on init
    // try to load fs from disk
    logging(LOG_INFO, "init", "Try to load fs from disk\n");
    read_buf(0);
    memcpy((uint8_t *) &superblock, (uint8_t *) buf, sizeof(superblock_t));

    // not on disk / corrupted
    if (!is_fs_avaliable()) {
        logging(LOG_WARNING, "init", "No fs found on disk, run mkfs\n");
        do_mkfs();
    }
}

int do_mkfs(void) {
    pcb_t *self = current_running[get_current_cpu_id()];
    logging(LOG_MAN, "fs", "%d.%s.%d run mkfs\n", self->pid, self->name, self->tid);

    logging(LOG_MAN, "fs", "Setting superblock\n");
    memset((void *) buf, 0, BLOCK_SIZE_BYTE);
    superblock.magic0 = superblock.magic1 = SUPERBLOCK_MAGIC;
    logging(LOG_MAN, "fs", "... magic=0x%lx\n", SUPERBLOCK_MAGIC);
    superblock.fs_start = FS_START;
    logging(LOG_MAN, "fs", "... start sector: %d\n", FS_START);
    superblock.inode_map_offset = 1;
    superblock.inode_map_size = MAX_INODE_MAP_SIZE;
    superblock.inode_num = 0;
    logging(LOG_MAN, "fs", "... inode map: offset=%d, size=%d\n", superblock.inode_map_size, superblock.inode_map_size);
    superblock.block_map_offset = superblock.inode_map_offset + superblock.inode_map_size;
    superblock.block_map_size = MAX_BLOCK_MAP_SIZE;
    superblock.block_num = 0;
    logging(LOG_MAN, "fs", "... block map: offset=%d, size=%d\n", superblock.block_map_offset, superblock.block_map_size);
    superblock.inode_offset = superblock.block_map_offset + superblock.block_map_size;
    uint32_t inode_size = MAX_INODE_NUM * sizeof(inode_t);
    logging(LOG_MAN, "fs", "... inode: offset=%d, size=%d\n", superblock.inode_offset, inode_size);
    superblock.data_offset = superblock.inode_offset + ROUND(inode_size, BLOCK_SIZE_BYTE);
    logging(LOG_MAN, "fs", "... data: offset=%d\n", superblock.data_offset);
    logging(LOG_MAN, "fs", "inode entry size: %d\n", sizeof(inode_t));
    logging(LOG_MAN, "fs", "directory entry size: %d\n", sizeof(dentry_t));


    memcpy((uint8_t *) buf, (uint8_t *) &superblock, sizeof(superblock_t));
    write_buf(0);

    // clear inode map, block map
    memset((void *) buf, 0, BLOCK_SIZE_BYTE);

    logging(LOG_MAN, "fs", "Setting inode map\n");
    for (uint32_t i=0; i<superblock.inode_map_size; i++)
        write_buf(superblock.inode_map_offset + i);

    logging(LOG_MAN, "fs", "Setting block map\n");
    for (uint32_t i=0; i<superblock.block_map_size; i++)
        write_buf(superblock.block_map_offset + i);

    // create root dir
    logging(LOG_MAN, "fs", "Creating root dir\n");
    do_mkdir("/");

    return 0;  // do_mkfs succeeds
}

int do_statfs(void) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "statfs: no file system found\n");
        return -1;
    }

    return 0;  // do_statfs succeeds
}

int do_cd(char *path) {
    // TODO [P6-task1]: Implement do_cd

    return 0;  // do_cd succeeds
}

int do_mkdir(char *path) {

    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path) {
    // TODO [P6-task1]: Implement do_rmdir

    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option) {
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core

    return 0;  // do_ls succeeds
}

int do_touch(char *path) {
    // TODO [P6-task2]: Implement do_touch

    return 0;  // do_touch succeeds
}

int do_cat(char *path) {
    // TODO [P6-task2]: Implement do_cat

    return 0;  // do_cat succeeds
}

int do_fopen(char *path, int mode) {
    // TODO [P6-task2]: Implement do_fopen

    return 0;  // return the id of file descriptor
}

int do_fread(int fd, char *buff, int length) {
    // TODO [P6-task2]: Implement do_fread

    return 0;  // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length) {
    // TODO [P6-task2]: Implement do_fwrite

    return 0;  // return the length of trully written data
}

int do_fclose(int fd) {
    // TODO [P6-task2]: Implement do_fclose

    return 0;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path) {
    // TODO [P6-task2]: Implement do_ln

    return 0;  // do_ln succeeds
}

int do_rm(char *path) {
    // TODO [P6-task2]: Implement do_rm

    return 0;  // do_rm succeeds
}

int do_lseek(int fd, int offset, int whence) {
    // TODO [P6-task2]: Implement do_lseek

    return 0;  // the resulting offset location from the beginning of the file
}
