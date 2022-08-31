#ifndef __INCLUDE_CONSOLE_H__
#define __INCLUDE_CONSOLE_H__

#include <type.h>
#include <os/bios.h>

char console_getchar(void);
int console_getline(char buf[], int bufsize);

#endif
