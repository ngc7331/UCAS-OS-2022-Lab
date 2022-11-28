#include <string.h>
#include <ctype.h>

void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len)
{
    for (; len != 0; len--) {
        *dest++ = *src++;
    }
}

void memset(void *dest, uint8_t val, uint32_t len)
{
    uint8_t *dst = (uint8_t *)dest;

    for (; len != 0; len--) {
        *dst++ = val;
    }
}

void bzero(void *dest, uint32_t len)
{
    memset(dest, 0, len);
}

int strlen(const char *src)
{
    int i = 0;
    while (src[i] != '\0') {
        i++;
    }
    return i;
}

int strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (*str1 != *str2) {
            return (*str1) - (*str2);
        }
        ++str1;
        ++str2;
    }
    return (*str1) - (*str2);
}

int strncmp(const char *str1, const char *str2, int n)
{
    while (*str1 && *str2 && n-- > 0) {
        if (*str1 != *str2) {
            return (*str1) - (*str2);
        }
        ++str1;
        ++str2;
    }
    return (*str1) - (*str2);
}

char *strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}

char *strncpy(char *dest, const char *src, int n)
{
    char *tmp = dest;

    while (*src && n-- > 0) {
        *dest++ = *src++;
    }

    while (n-- > 0) {
        *dest++ = '\0';
    }

    return tmp;
}

char *strcat(char *dest, const char *src)
{
    char *tmp = dest;

    while (*dest != '\0') {
        dest++;
    }
    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}


void strrev(char *str)
{
    int i, j;
    int len = strlen(str);

    for (i = 0, j = len - 1; i < j; i++, j--)
    {
        char tmp = str[i];
        str[i] = str[j];
        str[j] = tmp;
    }
}

// remove spaces from left end
int lstrip(char *s) {
    char *tmp = s;
    int len = 0;
    while (isspace(*s)) s++;
    for (; *s; len++, s++)
        tmp[len] = *s;
    tmp[len] = '\0';
    return len;
}

// remove spaces from right end
int rstrip(char *s) {
    char *tmp = s;
    int len = -1;
    for (int i=0; *s; i++, s++)
        if (!isspace(*s))
            len = i;
    tmp[len+1] = '\0';
    return len+1;
}

// remove spaces from both end
int strip(char *s) {
    lstrip(s);
    return rstrip(s);
}

void lower(char *s) {
    while (*s) {
        if (*s >= 'A' && *s <= 'Z')
            *s += 32;
        s ++;
    }
}
