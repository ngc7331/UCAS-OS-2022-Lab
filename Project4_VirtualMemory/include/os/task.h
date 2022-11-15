#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define APP_MAXNUM       64
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
    uint64_t size;
    uint64_t phyaddr;
    uint64_t entrypoint;
    uint64_t memsize;
    int execute_on_load;
    int loaded;
} task_info_t;

extern task_info_t apps[APP_MAXNUM];
extern short appnum;
extern task_info_t batchs[BATCH_MAXNUM];
extern short batchnum;

int get_taskid_by_name(char *name, task_type_t type);
void do_task_show(void);

#endif