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
    for (int i=0; i<10; i++) {
        sys_move_cursor(0, 0);
        sys_clear();
        printf("Round %d:\n", i);
        for (unsigned long j=base; j<base+ACCESS_NUM*PAGE_SIZE; j+=PAGE_SIZE) {
            printf("  access 0x%lx ... ", j);
            *(volatile unsigned long *) j = MAGIC;
            unsigned long tmp = *(volatile unsigned long *) j;
            assert(tmp == MAGIC);
            printf(" Done\n");
        }
    }
    printf("Success!\n");
    return 0;
}
