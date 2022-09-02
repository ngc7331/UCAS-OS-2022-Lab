#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

uint64_t load_img(uint64_t memaddr, uint64_t phyaddr, unsigned int size, int copy);
uint64_t load_task_img(int taskid);

#endif