#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SHM_PAGE_MAGIC 997
#define BARRIER_MAGIC 998
#define PRINT_LOC_BASE 0

typedef struct {
    char mbox_name[32];
    int len[8];
    int valid[8];
    int magic;
} shm_page_t;

void clearline(int line) {
    sys_move_cursor(0, line);
    for (int i=0; i<50; i++) {
        printf(" ");
    }
    sys_move_cursor(0, line);
}

void father(int argc, char *argv[]) {
    shm_page_t *shm = (shm_page_t *) sys_shmpageget(SHM_PAGE_MAGIC);

    // create a mailbox and send the name to child using shm
    char *mbox_name = "mailbox";
    int mbox = sys_mbox_open(mbox_name);
    shm->magic = SHM_PAGE_MAGIC;
    strcpy(shm->mbox_name, mbox_name);

    sys_move_cursor(0, PRINT_LOC_BASE);
    printf("father inited\n");

    int buff_size = atoi(argv[2]);
    int total = 0;
    char buf[buff_size+32];

    int fd = sys_fopen(argv[1], O_RDWR);

    for (int i=0; i<80; i++) {
        clearline(PRINT_LOC_BASE);
        printf("father waiting for message\n");

        int len = -1;
        while (len == -1) {
            for (int j=0; j<8; j++) {
                if (shm->valid[j]) {
                    len = shm->len[j];
                    shm->valid[j] = 0;
                    break;
                }
            }
        }

        sys_mbox_recv(mbox, buf+total, len);
        clearline(PRINT_LOC_BASE);
        printf("father received %d bytes message\n", len);
        total += len;
        if (total >= buff_size) {
            char time[10];
            itoa(sys_get_tick()/sys_get_timebase(), time, 10, 10);
            sys_fwrite(fd, time, 10);
            sys_fwrite(fd, "\n", 1);
            total = 0;
        }
    }

    sys_fclose(fd);

}

volatile shm_page_t *shm;

void child_thread(void *arg) {
    int id = (int)((long) arg);
    int barr = sys_barrier_init(BARRIER_MAGIC, 8);
    int mbox = sys_mbox_open(shm->mbox_name);

    for (int i=0; i<10; i++) {
        clearline(PRINT_LOC_BASE+2+id);
        printf("child[%d] waiting for barrier\n", id);

        sys_barrier_wait(barr);

        clearline(PRINT_LOC_BASE+2+id);
        printf("child[%d] waiting for recv\n", id);
        while (shm->valid[id])
            sys_yield();

        char msg[32];
        shm->len[id] = rand() % 32;
        clearline(PRINT_LOC_BASE+2+id);
        printf("child[%d] sending %d bytes message\n", id, shm->len[id]);
        sys_mbox_send(mbox, msg, shm->len[id]);
        shm->valid[id] = 1;

        clearline(PRINT_LOC_BASE+2+id);
        printf("child[%d] send complete, sleep(1)\n", id);
        sys_sleep(1);
    }

    sys_pthread_exit();
}

void child(int argc, char *argv[]) {
    shm = (shm_page_t *) sys_shmpageget(SHM_PAGE_MAGIC);

    sys_move_cursor(0, PRINT_LOC_BASE+1);
    printf("child: waiting for mbox_name\n");

    while (shm->magic != SHM_PAGE_MAGIC)
        sys_yield();

    srand(clock());

    pid_t tid[8];
    for (int i=0; i<8; i++) {
        clearline(PRINT_LOC_BASE+1);
        printf("child: creating thread[%d]\n", i);
        tid[i] = sys_pthread_create((uint64_t) child_thread, (void *)((long) i));
    }
    for (int i=0; i<8; i++) {
        clearline(PRINT_LOC_BASE+1);
        printf("child: waiting thread[%d]\n", i);
        sys_pthread_join(tid[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: exec test <log_file> <buffer_size>");
        return 0;
    }

    if (argc == 3) {
        // FIXME: replace with sys_fork()
        char *father_argv[] = {
            argv[0], argv[1], argv[2], "father"
        };
        char *child_argv[] = {
            argv[0], argv[1], argv[2], "child"
        };
        sys_exec(argv[0], 4, father_argv);
        sys_exec(argv[0], 4, child_argv);
    } else {
        if (strcmp(argv[3], "father") == 0) {
            father(argc-1, argv);
        } else if (strcmp(argv[3], "child") == 0) {
            child(argc-1, argv);
        } else {
            return 0;
        }
    }

    return 0;
}