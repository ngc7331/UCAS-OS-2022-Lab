#include <os/string.h>
#include <os/task.h>

int get_taskid_by_name(char *name, task_type_t type) {
    task_info_t *tasks = type == APP ? apps : batchs;
    int num = type == APP ? appnum : batchnum;
    for (int i=0; i<num; i++) {
        if (strcmp(name, tasks[i].name) == 0)
            return i;
    }
    return -1;
}
