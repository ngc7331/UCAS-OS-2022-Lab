#include <os/console.h>

char console_getchar(void) {
    int ch;
    while ((ch=bios_getchar()) == -1);  // ignore -1 (no input)
    bios_putchar(ch);  // echo
    return ch;
}

int console_getline(char buf[], int bufsize) {
    int i = 0;
    while ((buf[i]=console_getchar()) != '\r' && buf[i] != '\n' && i < bufsize-1) i++;
    bios_putstr("\n\r");  // echo
    buf[i] = '\0';
    return i;
}
