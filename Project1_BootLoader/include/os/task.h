#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000

// task_info_t
typedef struct {
    char name[32];
    uint64_t entrypoint;
    int size;
    int phyaddr;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];
extern short tasknum;

int get_taskid_by_name(char *taskname);

#endif