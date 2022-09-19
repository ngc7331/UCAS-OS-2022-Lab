#ifndef __INCLUDE_CONSOLE_H__
#define __INCLUDE_CONSOLE_H__

#include <os/bios.h>
#include <os/string.h>
#include <type.h>

char console_getchar(void);
char console_parsechar(int enable_backspace);
int console_getline(char buf[], int bufsize);
int str_format(char *fmt, char *buf, int len, char *val);
void console_print(char *fmt, char *val);

#endif
