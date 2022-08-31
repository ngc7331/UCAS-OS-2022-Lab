#include <os/console.h>

char console_getchar(void) {
    int ch;
    while ((ch=bios_getchar()) == -1);  // ignore -1 (no input)
    switch (ch) {
    case '\b': case 127:   // backspace
        // NOTE: this is for qemu in linux, may should be changed on borad
        // "\33[K": clear the content from the cursor to the end
        bios_putstr("\b\33[K");
        return 127;
    case '\n': case '\r':  // new line
        bios_putstr("\n\r");
        return ch;
    default:               // echo
        bios_putchar(ch);
        return ch;
    }
}

int console_getline(char buf[], int bufsize) {
    int i = 0;
    while ((buf[i]=console_getchar()) != '\r' && buf[i] != '\n' && i < bufsize-1) {
        if (buf[i] == 127) i--;
        else i++;
    }
    buf[i] = '\0';
    return i;
}

void str_format(char *fmt, char *buf, int len, char *val) {
    for (int i=0; i<len; i++) {
        buf[i] = fmt[i];
        if (fmt[i] == '_') {
            buf[i] = *val;
            val++;
        }
    }
}

void console_print(char *fmt, char *val) {
    int len = strlen(fmt);
    char buf[len+1];
    str_format(fmt, buf, len, val);
    buf[len] = '\0';
    bios_putstr(buf);
}
