#include <os/kernel.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/mm.h>

#define SECTOR_SIZE 512
#define MAX_SECTOR_READ 64
#define MAX_BYTE_READ (SECTOR_SIZE * MAX_SECTOR_READ)

uint64_t load_img(uint64_t memaddr, uint64_t phyaddr, uint64_t size) {
    static uintptr_t buff = 0;
    if (buff == 0)
        buff = allocPage(SECTOR_SIZE * MAX_SECTOR_READ / PAGE_SIZE);
    uint64_t memaddr_ori = memaddr;

    uint64_t block_id = phyaddr / SECTOR_SIZE;

    // the first byte's offset in the first block
    uint64_t offset = phyaddr % SECTOR_SIZE;

    /* load how many sectors
     * (size+offset) / SECTOR_SIZE are sectors which only contain the task and the head sector
     * if the remaining != 0 -> there's a tail sector
     */
    int num_of_blocks = ((size + offset) / SECTOR_SIZE)
                      + ((size + offset) % SECTOR_SIZE ? 1 : 0);

    // load
    while (num_of_blocks > 0) {
        if (bios_sdread(kva2pa(buff), num_of_blocks > MAX_SECTOR_READ ? MAX_SECTOR_READ : num_of_blocks, block_id) != 0) {
            return 0;
        }
        num_of_blocks -= MAX_SECTOR_READ;
        block_id += MAX_SECTOR_READ;
        if (memaddr) {
            int len = size > MAX_BYTE_READ - offset ? MAX_BYTE_READ - offset : size;
            memcpy((uint8_t *) memaddr, (uint8_t *) (buff + offset), len);
            memaddr += len;
            size -= len;
            offset = 0; // clear offset after first copy
        } else {
            // can only read once if not copy
            return buff + offset;
        }
    }
    return memaddr_ori;
}

uint64_t load_img_tmp(uint64_t phyaddr, uint64_t size) {
    return load_img(0, phyaddr, size);
}
