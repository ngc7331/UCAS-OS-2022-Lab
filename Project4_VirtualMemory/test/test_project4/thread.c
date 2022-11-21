#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// NOTE: please ensure that N can be divided by THREAD_NUM
#define N 2000
#define THREAD_NUM 4
#define STEP (N/THREAD_NUM)

int arr[N];
int res[THREAD_NUM];

int print_location;

static char blank[] = {"                                                                   "};

void worker(void *arg) {
    int self = (int)((long) arg);
    int sum = 0;
    int base = self * STEP;

    // add
    for (int i=0; i<STEP; i++) {
        sys_move_cursor(0, print_location + self + 1);
        printf(">        thread#%d   : add (%d/%d), partsum=%d\n", self, i, STEP, sum);
        sum += arr[i+base];
    }

    // done
    sys_move_cursor(0, print_location + self + 1);
    printf(blank);
    sys_move_cursor(0, print_location + self + 1);
    printf(">        thread#%d   : done, sum=%d\n", self, sum);

    res[self] = sum;
    pthread_exit();
}

int main(int argc, char **argv) {
    print_location = argc >=2 ? 0 : atoi(argv[1]);
    int sum = 0;
    pid_t tid[THREAD_NUM];

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
        pthread_create(&tid[i], worker, (void *)(long)i);
    }

    // wait until threads done
    sys_move_cursor(0, print_location);
    printf(blank);
    for (int i=0; i<THREAD_NUM; i++) {
        sys_move_cursor(0, print_location);
        printf("> [TASK] main thread: waiting for thread#%d\n", i);
        pthread_join(tid[i]);
    }

    // sum all
    for (int i=0; i<THREAD_NUM; i++) {
        sys_move_cursor(0, print_location);
        printf("> [TASK] main thread: sum a threads (%d/%d)\n", i, THREAD_NUM);
        sum += res[i];
    }

    // done
    sys_move_cursor(0, print_location);
    printf(blank);
    sys_move_cursor(0, print_location);
    printf("> [TASK] main thread: all done, sum=%d\n", sum);

    return 0;
}
