#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SHM_KEY 233
#define MUTEX_KEY 114514 
#define COND_KEY_FULL 1919
#define COND_KEY_EMPTY 810

typedef struct {
    int length;
    uint8_t buf[2048];
} shm_buf_t;

void clearline(int line) {
    sys_move_cursor(8, line);
    printf("                                                  ");
    sys_move_cursor(8, line);
}

void recv() {
    sys_move_cursor(0, 1);
    printf("[recv] loaded, init ...");
    shm_buf_t *shm = (shm_buf_t *) sys_shmpageget(SHM_KEY);
    int mutex = sys_mutex_init(MUTEX_KEY);
    int full = sys_condition_init(COND_KEY_FULL);
    int empty = sys_condition_init(COND_KEY_EMPTY);
    int round = 0;
    printf("done\n");

    sys_move_cursor(0, 1);
    printf("[recv] #");
    while (1) {
        clearline(1);
        printf("%d: ", round);
        sys_mutex_acquire(mutex);

        // wait until buffer is empty
        printf("waiting ... ");
        while (shm->length != 0)
            sys_condition_wait(empty, mutex);

        // recv from net
        printf("receiving ... ");
        int lens[1];
        shm->length = sys_net_recv(&shm->buf, 1, lens);

        // wake up sender
        printf("done\n");
        sys_condition_signal(full);

        sys_mutex_release(mutex);
        round ++;
    }

    // never exit
    sys_shmpagedt((void *) shm);
    sys_condition_destroy(COND_KEY_FULL);
}

void send() {
    sys_move_cursor(0, 2);
    printf("[send] loaded, init ...");
    shm_buf_t *shm = (shm_buf_t *) sys_shmpageget(SHM_KEY);
    int mutex = sys_mutex_init(MUTEX_KEY);
    int full = sys_condition_init(COND_KEY_FULL);
    int empty = sys_condition_init(COND_KEY_EMPTY);
    int round = 0;
    printf("done\n");

    sys_move_cursor(0, 2);
    printf("[send] #");
    while (1) {
        clearline(2);
        printf("%d: ", round);
        sys_mutex_acquire(mutex);

        // wait until buffer is empty
        printf("waiting ... ");
        while (shm->length == 0)
            sys_condition_wait(full, mutex);

        // recv from net
        printf("sending ... ");
        // replace target MAC to broadcast
        shm->buf[0] = 0xff;
        shm->buf[1] = 0xff;
        shm->buf[2] = 0xff;
        shm->buf[3] = 0xff;
        shm->buf[4] = 0xff;
        shm->buf[5] = 0xff;
        // replace "Requests:" to "Response:"
        for (int i=6; i<shm->length; i++) {
            if (strncmp("Requests:", shm->buf+i, 8) == 0) {
                shm->buf[i] = 'R';
                shm->buf[i+1] = 'e';
                shm->buf[i+2] = 's';
                shm->buf[i+3] = 'p';
                shm->buf[i+4] = 'o';
                shm->buf[i+5] = 'n';
                shm->buf[i+6] = 's';
                shm->buf[i+7] = 'e';
                break ;
            }
        }
        sys_net_send(&shm->buf, shm->length);
        shm->length = 0;

        // wake up sender
        printf("done\n");
        sys_condition_signal(empty);

        sys_mutex_release(mutex);
        round ++;
    }

    // never exit
    sys_shmpagedt((void *) shm);
    sys_condition_destroy(COND_KEY_EMPTY);
}

int main(int argc, char *argv[]) {
    sys_move_cursor(0, 0);
    if (argc == 1) {
        char *argv_send[] = {"echo", "0"};
        char *argv_recv[] = {"echo", "1"};
        printf("recv ...");
        sys_exec("echo", 2, argv_recv);
        printf(" ready; send ...");
        sys_exec("echo", 2, argv_send);
        printf(" ready;\n");
    } else {
        int self = atoi(argv[1]);
        if (self == 0)
            recv();
        else if (self == 1)
            send();
        else
            printf("error: invalid argument");
    }
    return 0;
}
