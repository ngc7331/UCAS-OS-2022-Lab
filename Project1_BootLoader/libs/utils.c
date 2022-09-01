#include <os/string.h>
#include <os/utils.h>

int is_space(char c) {
    return c == ' ' || c == '\t';
}

int atoi(char *a) {
    int len = strlen(a);
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
        bios_putstr("[kernel] W: atoi: no valid number input, returning 0\n\r");

    return neg ? -i : i;
}

void itoa(int i, int radix, char *a, int len, int zero_pad) {
    if (len < 4) {
        bios_putstr("[kernel] W: itoa: buffer too small");
        return;
    }

    if (i < 0) {
        *a++ = '-';
        len--;
        i = -i;
    }

    switch (radix) {  // add prefix for radix=2/8/16
    case 2:
        *a++ = '0'; *a++ = 'b'; len-=2; break;
    case 8:
        *a++ = '0'; *a++ = 'o'; len-=2; break;
    case 16:
        *a++ = '0'; *a++ = 'x'; len-=2; break;
    case 10: break;
    default:
        bios_putstr("[kernel] W: itoa: unsupported radix");
        return;
    }

    char buf[len];
    buf[--len] = '\0';

    do  {
        if (radix == 16 && i%radix >= 10) {
            buf[--len] = 'a' + i % radix - 10;
        } else {
            buf[--len] = '0' + i % radix;
        }
        i /= radix;
        zero_pad--;
    } while (i && len);

    while(zero_pad-- > 0 && len)
        buf[--len] = '0';

    while (buf[len])
        *a++ = buf[len++];

    *a = '\0';
}
