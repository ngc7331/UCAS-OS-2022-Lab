#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

#define SECTOR_SIZE 512

uint64_t load_task_img(int taskid) {
    // load task via taskid, which is converted by kernel
    task_info_t task = tasks[taskid];

    // load to mem_addr
    uint64_t mem_addr = task.entrypoint;

    // the first block containing task
    int block_id = task.phyaddr / SECTOR_SIZE;

    // the first byte's offset in the first block
    int offset = task.phyaddr % SECTOR_SIZE;

    /* load how many sectors
     * (size+offset) / SECTOR_SIZE are sectors which only contain the task and the head sector
     * if the remaining != 0 -> there's a tail sector
     */
    int sectors = ((task.size + offset) / SECTOR_SIZE)
                + ((task.size + offset) % SECTOR_SIZE ? 1 : 0);

    // load without padding
    if (bios_sdread(mem_addr, sectors, block_id) != 0) {
        return 0;
    }

    // copy data from (mem_addr + offset) to (mem_addr)
    // copy 8 bytes at once
    uint64_t *from = (uint64_t *) (mem_addr + offset);
    uint64_t *to = (uint64_t *) mem_addr;
    int rounds = task.size / sizeof(uint64_t) + 1;
    for (int i = 0; i < rounds; i++) {
        *to++ = *from++;
    }

    return mem_addr;
}
