#include <os/utils.h>

int is_space(char c) {
    return c == ' ' || c == '\t';
}

int atoi(char *a, int len) {
    int i=0, neg=0, num=0;
    while (is_space(*a) && len--) a++;  // skip

    if (*a == '-') neg = 1;

    while (len--) {
        if (*a>='0' && *a<='9') {
            i *= 10;
            i += *a - '0';
            num = 1;
        }
        a++;
    }

    if (!num)
        bios_putstr("[kernel] W: no valid number input, returning 0\n\r");

    return neg ? -i : i;
}
