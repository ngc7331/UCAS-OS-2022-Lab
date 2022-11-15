#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <os/task.h>

uint64_t load_img(uint64_t memaddr, uint64_t phyaddr, uint64_t size);
uint64_t load_img_tmp(uint64_t phyaddr, uint64_t size);

#endif