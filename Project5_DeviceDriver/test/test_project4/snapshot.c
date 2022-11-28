#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define MAGIC 0x114514

int main(int argc, char *argv[]) {
    unsigned long base = argc>1 ? atoi(argv[1]) : 0x10000000;
    unsigned long snapshot[10];
    unsigned long magic = MAGIC;
    srand(clock());
    *(long *) base = magic;
    for (int i=0; i<10; i++) {
        snapshot[i] = sys_snapshot(base);
        if (rand() % 100 >= 50) {
            *(long *) base = magic + i + 1;
        }
    }
    for (int i=0; i<10; i++) {
        printf("snapshot[%d] @ 0x%lx: 0x%lx @ 0x%lx\n", i, snapshot[i], *(long *) snapshot[i], sys_getpa(snapshot[i]));
    }
    printf("base        @ 0x%lx: 0x%lx @ 0x%lx\n", base, *(long *) base, sys_getpa(base));
    return 0;
}
