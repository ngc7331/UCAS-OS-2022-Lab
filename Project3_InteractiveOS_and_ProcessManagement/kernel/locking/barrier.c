#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/list.h>
#include <os/irq.h>
#include <atomic.h>
#include <printk.h>

barrier_t bars[BARRIER_NUM];

void init_barriers(void) {
    for (int i=0; i<BARRIER_NUM; i++) {
        bars[i].goal = bars[i].now = 0;
        list_init(&bars[i].block_queue);
        // bars[i].pid = 0;
        bars[i].allocated = 0;
    }
}

int do_barrier_init(int key, int goal) {
    int cid = get_current_cpu_id();
    // disable_preempt();
    // initialize barrier
    int idx = -1;
    // key has been allocated with a barrier
    for (int i=0; i<BARRIER_NUM; i++) {
        if (bars[i].key == key) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        // allocate a new barrier
        for (int i=0; i<BARRIER_NUM; i++) {
            if (bars[i].allocated == 0) {
                idx = i;
                break;
            }
        }
    }
    if (idx >= 0) {
        // allocate success
        bars[idx].allocated ++;
        bars[idx].key = key;
        bars[idx].goal = goal;
        logging(LOG_INFO, "locking", "%d.%s.%d get barrier[%d] with key=%d, goal=%d\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, idx, key, goal);
    } else {
        // allocate failed
        logging(LOG_WARNING, "locking", "%d.%s.%d init barrier failed\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid);
    }
    // enable_preempt();
    return idx;
}

void do_barrier_wait(int bar_idx) {
    int cid = get_current_cpu_id();
    if (++bars[bar_idx].now >= bars[bar_idx].goal) {
        // reached goal, unblock all
        logging(LOG_INFO, "locking", "%d.%s.%d reached barrier[%d], goal!\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, bar_idx);
        bars[bar_idx].now = 0;
        while (!list_is_empty(&bars[bar_idx].block_queue))
            do_unblock(&bars[bar_idx].block_queue);
    } else {
        logging(LOG_INFO, "locking", "%d.%s.%d reached barrier[%d], waiting (%d/%d)\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, bar_idx, bars[bar_idx].now, bars[bar_idx].goal);
        do_block(current_running[cid], &bars[bar_idx].block_queue);
    }
}

void do_barrier_destroy(int bar_idx) {
    int cid = get_current_cpu_id();
    logging(LOG_INFO, "locking", "%d.%s.%d destroy barrier[%d]\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, bar_idx);
    bars[bar_idx].allocated = 0;
    bars[bar_idx].now = bars[bar_idx].goal = 0;
    list_init(&bars[bar_idx].block_queue);
    // while (!list_is_empty(&bars[bar_idx].block_queue))
    //     do_unblock(&bars[bar_idx].block_queue);
}
