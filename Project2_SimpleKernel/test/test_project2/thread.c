#include <stdio.h>
#include <unistd.h>

// NOTE: please ensure that N can be divided by THREAD_NUM
#define N 2000
#define THREAD_NUM 4
#define STEP (N/THREAD_NUM)

int retval[THREAD_NUM];
int arr[N];

static char blank[] = {"                                                                   "};

void worker(void *arg) {
    int self = (int) arg;
    int print_location = 7 + self;
    int sum = 0;
    int base = self * STEP;

    // add
    for (int i=0; i<STEP; i++) {
        sys_move_cursor(0, print_location);
        printf(">        thread#%d   : add (%d/%d)\n", self, i, STEP);
        sum += arr[i+base];
    }

    // done
    sys_move_cursor(0, print_location);
    printf(blank);
    sys_move_cursor(0, print_location);
    printf(">        thread#%d   : done, sum=%d\n", self, sum);
    retval[self] = sum;

    while (1) sys_yield();
}

int main() {
    int print_location = 6;
    int sum = 0;

    for (int i=0; i<N; i++) {
        sys_move_cursor(0, print_location);
        printf("> [TASK] main thread: fillin numbers (%d/%d)\n", i, N);
        arr[i] = i+1;
    }

    sys_move_cursor(0, print_location);
    printf(blank);
    sys_move_cursor(0, print_location);
    printf("> [TASK] main thread: create threads, %ld\n", (uint64_t) worker);
    for (int i=0; i<THREAD_NUM; i++) {
        sys_thread_create((uint64_t) worker, (void *) i);
    }

    // wait until threads done
    sys_move_cursor(0, print_location);
    printf(blank);
    for (int i=0; i<THREAD_NUM; i++) {
        while (!retval[i]) {
            sys_move_cursor(0, print_location);
            printf("> [TASK] main thread: waiting for thread#%d\n", sum, i);
            sys_yield();
        }
    }

    // done
    for (int i=0; i<THREAD_NUM; i++) {
        sum += retval[i];
    }
    sys_move_cursor(0, print_location);
    printf(blank);
    sys_move_cursor(0, print_location);
    printf("> [TASK] main thread: all done, sum=%d\n", sum);

    while (1) sys_yield();

    return 0;
}
