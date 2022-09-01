#include <os/bios.h>
#include <os/string.h>
#include <os/task.h>

int get_taskid_by_name(char *taskname) {
    for (int i=0; i<tasknum; i++) {
        if (strcmp(taskname, tasks[i].name) == 0)
            return i;
    }
    return -1;
}
