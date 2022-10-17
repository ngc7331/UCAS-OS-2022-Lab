#include <os/kernel.h>
#include <os/loader.h>
#include <os/string.h>

#define SECTOR_SIZE 512

uint64_t load_img(uint64_t memaddr, uint64_t phyaddr, unsigned int size, int copy) {

    unsigned int block_id = phyaddr / SECTOR_SIZE;

    // the first byte's offset in the first block
    unsigned int offset = phyaddr % SECTOR_SIZE;

    /* load how many sectors
     * (size+offset) / SECTOR_SIZE are sectors which only contain the task and the head sector
     * if the remaining != 0 -> there's a tail sector
     */
    unsigned int num_of_blocks = ((size + offset) / SECTOR_SIZE)
                      + ((size + offset) % SECTOR_SIZE ? 1 : 0);

    // load
    if (bios_sdread(memaddr, num_of_blocks, block_id) != 0) {
        return 0;
    }

    if (copy) {
        // copy data from (mem_addr + offset) to (mem_addr)
        uint8_t *src = (uint8_t *) (memaddr + offset);
        uint8_t *dst = (uint8_t *) memaddr;
        memcpy(dst, src, size);
        return memaddr;
    } else {
        // doesn't copy, return the actual memaddr
        return memaddr + offset;
    }

}

uint64_t load_task_img(int taskid, task_type_t type) {
    // load task via taskid, which is converted by kernel
    task_info_t task = type == APP ? apps[taskid] : batchs[taskid];
    uint64_t memaddr = type == APP ? task.entrypoint : BATCH_MEM_BASE;

    return load_img(memaddr, task.phyaddr, task.size, type==APP);
}
