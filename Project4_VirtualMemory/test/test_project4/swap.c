#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PAGE_SIZE 0x1000
#define ACCESS_NUM 32
#define MAGIC 0x1919810

int main(int argc, char* argv[])
{
    unsigned long base = argc>1 ? atoi(argv[1]) : 0x10000000;
    sys_move_cursor(0, 0);
    sys_clear();
    for (unsigned long i=base; i<base+ACCESS_NUM*PAGE_SIZE; i+=PAGE_SIZE) {
        printf("0x%lx write ... ", i);
        *(volatile unsigned long *) i = MAGIC;
        printf(" Done\n");
    }
    sys_move_cursor(0, 0);
    for (unsigned long i=base; i<base+ACCESS_NUM*PAGE_SIZE; i+=PAGE_SIZE) {
        sys_move_cursor_r(30, 0);
        printf("read ... ", i);
        unsigned long tmp = *(volatile unsigned long *) i;
        assert(tmp == MAGIC);
        printf(" Done\n");
    }
    printf("Success!\n");
    return 0;
}
