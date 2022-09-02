#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

#define SECTOR_SIZE 512

uint64_t load_img(uint64_t memaddr, uint64_t phyaddr, unsigned int size, int copy) {

    unsigned int block_id = phyaddr / SECTOR_SIZE;

    // the first byte's offset in the first block
    unsigned int offset = phyaddr % SECTOR_SIZE;

    /* load how many sectors
     * (size+offset) / SECTOR_SIZE are sectors which only contain the task and the head sector
     * if the remaining != 0 -> there's a tail sector
     */
    int num_of_blocks = ((size + offset) / SECTOR_SIZE)
                      + ((size + offset) % SECTOR_SIZE ? 1 : 0);

    // load
    if (bios_sdread(memaddr, num_of_blocks, block_id) != 0) {
        return 0;
    }

    if (copy) {
        // copy data from (mem_addr + offset) to (mem_addr)
        // copy 8 bytes at once
        uint64_t *src = (uint64_t *) (memaddr + offset);
        uint64_t *dst = (uint64_t *) memaddr;
        int rounds = size / sizeof(uint64_t) + 1;
        for (int i = 0; i < rounds; i++) {
            *dst++ = *src++;
        }
        return memaddr;
    } else {
        return memaddr + offset;
    }

}

uint64_t load_task_img(int taskid) {
    // load task via taskid, which is converted by kernel
    task_info_t task = tasks[taskid];

    return load_img(task.entrypoint, task.phyaddr, task.size, TRUE);
}
