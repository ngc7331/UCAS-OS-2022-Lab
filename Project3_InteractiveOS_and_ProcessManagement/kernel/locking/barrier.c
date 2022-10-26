#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/irq.h>
#include <atomic.h>
#include <printk.h>

barrier_t barriers[BARRIER_NUM];

void init_barriers(void) {
    for (int i=0; i<BARRIER_NUM; i++) {
        barriers[i].goal = barriers[i].now = 0;
        list_init(&barriers[i].block_queue);
        // barriers[i].pid = 0;
        barriers[i].allocated = 0;
    }
}

int do_barrier_init(int key, int goal) {
    // disable_preempt();
    // initialize barrier
    int idx = -1;
    // key has been allocated with a barrier
    for (int i=0; i<BARRIER_NUM; i++) {
        if (barriers[i].key == key) {
            idx = i;
            break;
        }
    }
    // allocate a new barrier
    for (int i=0; i<BARRIER_NUM; i++) {
        if (barriers[i].allocated == 0) {
            idx = i;
            break;
        }
    }
    // allocate success
    if (idx >= 0) {
        barriers[idx].allocated ++;
        barriers[idx].key = key;
        barriers[idx].goal = goal;
        logging(LOG_INFO, "locking", "%d.%s.%d get barrier[%d] with key=%d, goal=%d\n",
                current_running->pid, current_running->name, current_running->tid, idx, key, goal);
    }
    // allocate failed
    // enable_preempt();
    return idx;
}

void do_barrier_wait(int bar_idx) {
    if (++barriers[bar_idx].now >= barriers[bar_idx].goal) {
        // reached goal, unblock all
        logging(LOG_INFO, "locking", "barrier[%d] reached goal\n", bar_idx);
        barriers[bar_idx].now = 0;
        while (!list_is_empty(&barriers[bar_idx].block_queue))
            do_unblock(&barriers[bar_idx].block_queue);
    } else {
        logging(LOG_INFO, "locking", "barrier[%d] waiting(%d/%d)\n", bar_idx, barriers[bar_idx].now, barriers[bar_idx].goal);
        do_block(current_running, &barriers[bar_idx].block_queue);
    }
}

void do_barrier_destroy(int bar_idx) {
    logging(LOG_INFO, "locking", "barrier[%d] destroyed\n", bar_idx);
    barriers[bar_idx].allocated = 0;
    barriers[bar_idx].now = barriers[bar_idx].goal = 0;
    while (!list_is_empty(&barriers[bar_idx].block_queue))
        do_unblock(&barriers[bar_idx].block_queue);
}
