#include <os/kernel.h>
#include <os/loader.h>
#include <os/string.h>

#define SECTOR_SIZE 512

uint64_t load_img(uint64_t memaddr, uint64_t phyaddr, uint64_t size, int copy) {

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
        if (bios_sdread(memaddr, num_of_blocks > 64 ? 64 : num_of_blocks, block_id) != 0) {
            return 0;
        }
        num_of_blocks -= 64;
        memaddr += 64 * SECTOR_SIZE;
        block_id += 64;
    }

    memaddr -= 64 * SECTOR_SIZE;

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

uint64_t load_task_img(int taskid, task_type_t type, int overwrite) {
    // load task via taskid, which is converted by kernel
    int is_app = type == APP;
    task_info_t *task = is_app ? &apps[taskid] : &batchs[taskid];
    // FIXME: memaddr should be allocated by kernel and use kvaddr
    uint64_t memaddr = is_app ? task->entrypoint : BATCH_MEM_BASE;

    // FIXME: batches?
    if (task->loaded && !overwrite)
        return memaddr;

    task->loaded = 1;
    return load_img(memaddr, task->phyaddr, task->size, is_app);
}
