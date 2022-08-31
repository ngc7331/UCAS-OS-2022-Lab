#include <os/utils.h>

int is_space(char c) {
    return c == ' ' || c == '\t';
}

int atoi(char *a, int len) {
    int i=0;
    int neg = 0;
    while (is_space(*a)) a++;  // skip
    if (*a == '-') neg = 1;
    while (len--) {
        if (*a>='0' && *a<='9') {
            i *= 10;
            i += *a - '0';
        }
        a++;
    }
    return neg ? -i : i;
}
