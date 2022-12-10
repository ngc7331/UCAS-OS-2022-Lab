#include <os/kernel.h>
#include <os/loader.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/fs.h>
#include <printk.h>

#define PRINT_ALIGN 8

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];

static uint8_t buf[2][BLOCK_SIZE_BYTE];

// this should always point to a DIR inode
static int current_ino;

static int is_fs_avaliable(void) {
    return superblock.magic0 == SUPERBLOCK_MAGIC && superblock.magic1 == SUPERBLOCK_MAGIC;
}

static int write_buf(uint32_t offset, int buf_id) {
    return bios_sdwrite(kva2pa((uint64_t) buf[buf_id]), BLOCK_SIZE, FS_START + offset);
}

static int read_buf(uint32_t offset, int buf_id) {
    return bios_sdread(kva2pa((uint64_t) buf[buf_id]), BLOCK_SIZE, FS_START + offset);
}

static int write_superblock(void) {
    return bios_sdwrite(kva2pa((uint64_t) &superblock), BLOCK_SIZE, FS_START);
}

static int _alloc_bitmap(int tp) {
    int map_num = tp == 0 ? superblock.inode_map_size : superblock.block_map_size;
    int map_base = tp == 0 ? superblock.inode_map_offset : superblock.block_map_offset;
    for (int i=0; i<map_num; i++) {              // which block
        read_buf(map_base + i, 0);
        for (int j=0; j<BLOCK_SIZE_BYTE; j++) {  // which byte
            if (buf[0][j] != 0xff) {
                for (int k=0; k<8; k++) {        // which bit
                    if (!(buf[0][j] & (1 << k))) {  // not used
                        buf[0][j] |= (1 << k);
                        write_buf(map_base + i, 0);
                        if (tp == 0)
                            superblock.inode_num ++;
                        else
                            superblock.block_num ++;
                        return (i * BLOCK_SIZE_BYTE + j) * 8 + k;
                    }
                }
            }
        }
    }
    return -1;
}

#define alloc_inode() _alloc_bitmap(0)
#define alloc_block() _alloc_bitmap(1)

static inode_t *get_inode(int ino) {
    int block = superblock.inode_offset + ino / BLOCK_SIZE_BYTE;
    int offset = ino % BLOCK_SIZE_BYTE;
    read_buf(block, 1);
    return ((inode_t *) &buf[1]) + offset;
}
static void write_inode(int ino) {
    int block = superblock.inode_offset + ino / BLOCK_SIZE_BYTE;
    write_buf(block, 1);
}

static void *get_block(int block) {
    read_buf(superblock.data_offset + block, 0);
    return (void *) buf[0];
}
static void write_block(int block) {
    write_buf(superblock.data_offset + block, 0);
}

static int parse_path(char *path, char **name) {
    // if name is NULL, return the ino of the path
    // else return the ino of the parent dir, and set name to the name of the file
    int ino;
    char *p;
    if (path[0] == '/') { // absolute path
        ino = 0;
        p = path + 1;
    } else {
        ino = current_ino;
        p = path;
    }
    // TODO
    return ino;
}

static void _mkdir(int ino, int pino) {
    int dir_block = alloc_block();
    logging(LOG_VERBOSE, "fs", "mkdir for ino=0x%x, pino=0x%x, block=0x%x\n", ino, pino, dir_block);
    inode_t *inode = get_inode(ino);
    inode->type = INODE_DIR;
    inode->link = 1;
    inode->size = BLOCK_SIZE_BYTE;
    inode->direct_blocks[0] = dir_block;
    for (int i=1; i<DIRECT_BLOCK_NUM; i++)
        inode->direct_blocks[i] = -1;
    inode->indirect_block = -1;
    write_inode(ino);
    dentry_t *dentry = (dentry_t *) get_block(dir_block);
    dentry[0].ino = ino;
    dentry[0].valid = 1;
    strcpy(dentry[0].name, ".");
    dentry[1].ino = pino;
    dentry[1].valid = 1;
    strcpy(dentry[1].name, "..");
    write_block(dir_block);
}

void init_fs(void) {
    // NOTE: this should be called only by kernel on init
    // try to load fs from disk
    logging(LOG_INFO, "init", "Try to load fs from disk\n");
    read_buf(0, 0);
    memcpy((uint8_t *) &superblock, (uint8_t *) buf[0], sizeof(superblock_t));

    // not on disk / corrupted
    if (!is_fs_avaliable()) {
        logging(LOG_WARNING, "init", "No fs found on disk, run mkfs\n");
        do_mkfs();
    }

    // if fs loaded, root dir's ino must bt 0
    current_ino = 0;
}

int do_mkfs(void) {
    pcb_t *self = current_running[get_current_cpu_id()];
    logging(LOG_INFO, "fs", "%d.%s.%d run mkfs\n", self->pid, self->name, self->tid);

    // set superblock
    logging(LOG_MAN, "fs", "Setting superblock\n");

    superblock.magic0 = superblock.magic1 = SUPERBLOCK_MAGIC;
    logging(LOG_MAN, "fs", "... magic=0x%x\n", SUPERBLOCK_MAGIC);

    superblock.fs_start = FS_START;
    logging(LOG_MAN, "fs", "... start sector: 0x%x\n", FS_START);

    superblock.inode_map_offset = 1;
    superblock.inode_map_size = MAX_INODE_MAP_SIZE;
    superblock.inode_num = 0;
    logging(LOG_MAN, "fs", "... inode map: offset=0x%x, size=0x%x\n", superblock.inode_map_size, superblock.inode_map_size);

    superblock.block_map_offset = superblock.inode_map_offset + superblock.inode_map_size;
    superblock.block_map_size = MAX_BLOCK_MAP_SIZE;
    superblock.block_num = 0;
    logging(LOG_MAN, "fs", "... block map: offset=0x%x, size=0x%x\n", superblock.block_map_offset, superblock.block_map_size);

    superblock.inode_offset = superblock.block_map_offset + superblock.block_map_size;
    uint32_t real_inode_size = MAX_INODE_NUM * sizeof(inode_t);
    uint32_t inode_size = ROUND(real_inode_size, BLOCK_SIZE_BYTE) / BLOCK_SIZE_BYTE;
    logging(LOG_MAN, "fs", "... inode: offset=0x%x, size=0x%x (%dB)\n", superblock.inode_offset, inode_size, real_inode_size);
    superblock.data_offset = superblock.inode_offset + inode_size;
    logging(LOG_MAN, "fs", "... data: offset=0x%x\n", superblock.data_offset);
    logging(LOG_MAN, "fs", "inode entry size: %dB\n", sizeof(inode_t));
    logging(LOG_MAN, "fs", "directory entry size: %dB\n", sizeof(dentry_t));

    // clear inode map, block map
    memset((void *) buf[0], 0, BLOCK_SIZE_BYTE);

    logging(LOG_MAN, "fs", "Setting inode map\n");
    for (uint32_t i=0; i<superblock.inode_map_size; i++)
        write_buf(superblock.inode_map_offset + i, 0);

    logging(LOG_MAN, "fs", "Setting block map\n");
    for (uint32_t i=0; i<superblock.block_map_size; i++)
        write_buf(superblock.block_map_offset + i, 0);

    // create root dir
    logging(LOG_MAN, "fs", "Creating root dir\n");
    current_ino = alloc_inode();
    _mkdir(current_ino, current_ino);

    // write superblock
    write_superblock();

    return 0;  // do_mkfs succeeds
}

int do_statfs(void) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "statfs: no file system found\n");
        return -1;
    }

    printk("magic       : 0x%x\n", superblock.magic0);
    printk("start sector: 0x%x\n", superblock.fs_start);
    printk("inode map   : offset=0x%x, size=0x%x\n", superblock.inode_map_offset, superblock.inode_map_size);
    printk("block map   : offset=0x%x, size=0x%x\n", superblock.block_map_offset, superblock.block_map_size);
    uint32_t real_inode_size = MAX_INODE_NUM * sizeof(inode_t);
    uint32_t inode_size = ROUND(real_inode_size, BLOCK_SIZE_BYTE) / BLOCK_SIZE_BYTE;
    printk("inode       : offset=0x%x, size=0x%x (%dB)\n", superblock.inode_offset, inode_size, real_inode_size);
    printk("data        : offset=0x%x\n", superblock.data_offset);
    printk("used inode %d / %d\n", superblock.inode_num, MAX_INODE_NUM);
    printk("used block %d / %d\n", superblock.block_num, MAX_BLOCK_NUM);
    printk("inode entry size: %dB\n", sizeof(inode_t));
    printk("directory entry size: %dB\n", sizeof(dentry_t));

    return 0;  // do_statfs succeeds
}

int do_cd(char *path) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "cd: no file system found\n");
        return -1;
    }
    // TODO [P6-task1]: Implement do_cd

    return 0;  // do_cd succeeds
}

int do_mkdir(char *path) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "mkdir: no file system found\n");
        return -1;
    }
    // TODO [P6-task1]: Implement do_mkdir

    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "rmdir: no file system found\n");
        return -1;
    }
    // TODO [P6-task1]: Implement do_rmdir

    return 0;  // do_rmdir succeeds
}

#define LS_OPTIONS_L 0x1
#define LS_OPTIONS_A 0x2

int do_ls(char *path, int option) {
    pcb_t *self = current_running[get_current_cpu_id()];
    logging(LOG_INFO, "fs", "%d.%s.%d do ls\n", self->pid, self->name, self->tid);
    logging(LOG_DEBUG, "fs", "... path=\"%s\", option=%d\n", path, option);
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "ls: no file system found\n");
        return -1;
    }
    // Note: argument 'option' serves for 'ls -l' in A-core
    int ino = parse_path(path, NULL);
    logging(LOG_DEBUG, "fs", "... ino=0x%x\n", ino);
    if (ino == -1) {
        logging(LOG_ERROR, "fs", "ls: path not found\n");
        return -1;
    }
    inode_t *inode = get_inode(ino);
    if (inode->type != INODE_DIR) {
        logging(LOG_ERROR, "fs", "ls: not a directory\n");
        return -1;
    }

    int detailed = option & LS_OPTIONS_L;
    int all = option & LS_OPTIONS_A;

    if (detailed) {
        printk("File list of %s\n", path);
        printk("ino | type | lnk |   size   | name\n");
    }

    for (int i=0; i<DIRECT_BLOCK_NUM; i++) {
        inode_t *inode = get_inode(ino);
        if (inode->direct_blocks[i] == -1)
            break;
        logging(LOG_DEBUG, "fs", "... direct_block[%d]: 0x%x\n", i, inode->direct_blocks[i]);
        dentry_t *dentry = (dentry_t *) get_block(inode->direct_blocks[i]);
        for (int j=0; j<BLOCK_SIZE_BYTE/sizeof(dentry_t); j++) {
            if (dentry[j].valid) {
                if (!all && dentry[j].name[0] == '.')
                    continue;
                if (detailed) {
                    inode_t *inode = get_inode(dentry[j].ino);
                    char *dict[] = {"FILE", "DIR "};
                    printk("%03d | %s | %03d | %08d | %s\n", dentry[j].ino, dict[inode->type], inode->link, inode->size, dentry[j].name);
                } else {
                    printk("%s ", dentry[j].name);
                    for (int k=strlen(dentry[j].name) % PRINT_ALIGN; k<PRINT_ALIGN; k++)
                        printk(" ");
                }
            }
        }
    }
    printk("\n");

    return 0;  // do_ls succeeds
}

int do_touch(char *path) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "touch: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_touch

    return 0;  // do_touch succeeds
}

int do_cat(char *path) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "cat: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_cat

    return 0;  // do_cat succeeds
}

int do_fopen(char *path, int mode) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "fopen: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_fopen

    return 0;  // return the id of file descriptor
}

int do_fread(int fd, char *buff, int length) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "fread: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_fread

    return 0;  // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "fwrite: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_fwrite

    return 0;  // return the length of trully written data
}

int do_fclose(int fd) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "fclose: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_fclose

    return 0;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "ln: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_ln

    return 0;  // do_ln succeeds
}

int do_rm(char *path) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "rm: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_rm

    return 0;  // do_rm succeeds
}

int do_lseek(int fd, int offset, int whence) {
    if (!is_fs_avaliable()) {
        logging(LOG_ERROR, "fs", "lseek: no file system found\n");
        return -1;
    }
    // TODO [P6-task2]: Implement do_lseek

    return 0;  // the resulting offset location from the beginning of the file
}
