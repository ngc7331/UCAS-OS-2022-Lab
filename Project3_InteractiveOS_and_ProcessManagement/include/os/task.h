#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define APP_MEM_BASE     0x52000000
#define APP_MAXNUM       64
#define APP_SIZE         0x10000

#define BATCH_MEM_BASE   (APP_MEM_BASE + APP_MAXNUM * APP_SIZE)
#define BATCH_MAXNUM     16

// task type
typedef enum {
    APP,
    BATCH
} task_type_t;

// task info
typedef struct {
    char name[32];
    task_type_t type;
    int size;
    int phyaddr;
    uint64_t entrypoint;
    int execute_on_load;
} task_info_t;

extern task_info_t apps[APP_MAXNUM];
extern short appnum;
extern task_info_t batchs[BATCH_MAXNUM];
extern short batchnum;

int get_taskid_by_name(char *name, task_type_t type);

#endif