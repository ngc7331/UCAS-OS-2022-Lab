#ifndef __INCLUDE_UTILS_H__
#define __INCLUDE_UTILS_H__

#include <os/bios.h>
#include <type.h>

int is_space(char c);
int atoi(char *a);
void itoa(int i, int radix, char *a, int len, int zero_pad);

#endif
