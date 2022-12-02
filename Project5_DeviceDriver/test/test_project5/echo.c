#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SHM_KEY 233
#define MUTEX_KEY 114514 
#define COND_KEY_FULL 1919
#define COND_KEY_EMPTY 810
#define BUFNUM 8
#define BUFSIZE 256

typedef struct {
    int rp, sp;
    int length[BUFNUM];
    char buf[BUFNUM][BUFSIZE];
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
    int not_empty = sys_condition_init(COND_KEY_FULL);
    int not_full = sys_condition_init(COND_KEY_EMPTY);
    int round = 0;
    printf("done\n");

    sys_move_cursor(0, 1);
    printf("[recv] #");
    while (1) {
        clearline(1);
        printf("%d: rp=%d, process=", round, shm->rp);
        sys_mutex_acquire(mutex);

        // wait until buffer is not full
        while ((shm->sp+1) % BUFNUM == shm->rp) {
            printf("W");
            sys_condition_wait(not_full, mutex);
        }

        // recv from net
        printf("R");
        int lens[1];
        int rp = shm->rp;
        shm->length[rp] = sys_net_recv(&shm->buf[rp], 1, lens);
        shm->rp = (rp + 1) % BUFNUM;

        // wake up sender
        printf("D\n");
        sys_condition_signal(not_empty);

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
    int not_empty = sys_condition_init(COND_KEY_FULL);
    int not_full = sys_condition_init(COND_KEY_EMPTY);
    int round = 0;
    printf("done\n");

    sys_move_cursor(0, 2);
    printf("[send] #");
    while (1) {
        clearline(2);
        printf("%d: sp=%d, process=", round, shm->sp);
        sys_mutex_acquire(mutex);

        // wait until buffer is not empty
        while (shm->sp == shm->rp) {
            printf("W");
            sys_condition_wait(not_empty, mutex);
        }

        // recv from net
        printf("S");
        int sp = shm->sp;
        // replace target MAC to broadcast
        shm->buf[sp][0] = 0xff;
        shm->buf[sp][1] = 0xff;
        shm->buf[sp][2] = 0xff;
        shm->buf[sp][3] = 0xff;
        shm->buf[sp][4] = 0xff;
        shm->buf[sp][5] = 0xff;
        // replace "Requests:" to "Response:"
        for (int i=6; i<shm->length[sp]; i++) {
            if (strncmp("Requests:", shm->buf[sp]+i, 8) == 0) {
                shm->buf[sp][i] = 'R';
                shm->buf[sp][i+1] = 'e';
                shm->buf[sp][i+2] = 's';
                shm->buf[sp][i+3] = 'p';
                shm->buf[sp][i+4] = 'o';
                shm->buf[sp][i+5] = 'n';
                shm->buf[sp][i+6] = 's';
                shm->buf[sp][i+7] = 'e';
                break ;
            }
        }
        sys_net_send(&shm->buf[sp], shm->length[sp]);
        shm->sp = (sp + 1) % BUFNUM;

        // wake up sender
        printf("D\n");
        sys_condition_signal(not_full);

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
