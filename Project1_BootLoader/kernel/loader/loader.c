#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

#define PADDING_SECTORS   16  // TODO: [task4] this should be deleted
#define SECTOR_SIZE 512

uint64_t load_task_img(int taskid) {
    // load task via taskid, which is converted by kernel
    unsigned int mem_addr = tasks[taskid].entrypoint;
    unsigned int block_id = tasks[taskid].phyaddr / SECTOR_SIZE;

    // TODO: [task4] load without padding
    if (bios_sdread(mem_addr, PADDING_SECTORS, block_id) != 0)
        return 0;

    return mem_addr;
}
