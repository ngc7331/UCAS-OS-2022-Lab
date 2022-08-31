#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

#define PADDING_SECTORS   16  // TODO: [task4] this should be deleted

uint64_t load_task_img(int taskid) {
    // TODO: [p1-task4] load task via task name, thus the arg should be 'char *taskname'

    // load task by id
    unsigned int mem_addr = TASK_MEM_BASE + taskid * TASK_SIZE;
    unsigned int block_id = 1 + (taskid + 1) * PADDING_SECTORS;

    if (bios_sdread(mem_addr, PADDING_SECTORS, block_id) != 0)
        return 0;

    return mem_addr;
}
