#ifndef __INCLUDE_UTILS_H__
#define __INCLUDE_UTILS_H__

#include <os/kernel.h>
#include <type.h>

int is_space(char c);
int atoi(char *a);
void itoa(int i, int radix, char *a, int len, int zero_pad);
int lstrip(char *s);
int rstrip(char *s);
int strip(char *s);

#endif
