#ifndef __INCLUDE_BATCH_H__
#define __INCLUDE_BATCH_H__

#include <type.h>
#include <os/bios.h>
#include <os/console.h>
#include <os/loader.h>
#include <os/task.h>

#define BATCH_MEM_BASE    0x53000000
#define BATCH_MAXNUM      16

typedef struct {
    char name[32];
    int size;
    int phyaddr;
    int execute_on_load;
} batch_info_t;

extern batch_info_t batchs[BATCH_MAXNUM];
extern short batchnum;

int batch_execute(int batchid);
int get_batchid_by_name(char *name);

#endif
