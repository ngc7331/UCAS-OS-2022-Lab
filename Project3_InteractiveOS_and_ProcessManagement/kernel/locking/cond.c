#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/irq.h>
#include <atomic.h>
#include <printk.h>

condition_t conds[CONDITION_NUM];

void init_conditions(void) {
    for (int i=0; i<BARRIER_NUM; i++) {
        list_init(&conds[i].block_queue);
        conds[i].allocated = 0;
    }
}

int do_condition_init(int key) {
    // disable_preempt();
    // initialize condition
    int idx = -1;
    // key has been allocated with a cond
    for (int i=0; i<CONDITION_NUM; i++) {
        if (conds[i].key == key) {
            idx = i;
            break;
        }
    }
    // allocate a new cond
    for (int i=0; i<CONDITION_NUM; i++) {
        if (conds[i].allocated == 0) {
            idx = i;
            break;
        }
    }
    // allocate success
    if (idx >= 0) {
        conds[idx].allocated ++;
        conds[idx].key = key;
        logging(LOG_INFO, "locking", "%d.%s.%d get condition[%d] with key=%d\n",
                current_running->pid, current_running->name, current_running->tid, idx, key);
    }
    // allocate failed
    // enable_preempt();
    return idx;
}

void do_condition_wait(int cond_idx, int mutex_idx) {
    logging(LOG_INFO, "locking", "%d.%s.%d waiting for condition[%d] with mlock[%d]\n",
            current_running->pid, current_running->name, current_running->tid, cond_idx, mutex_idx);
    do_mutex_lock_release(mutex_idx);
    do_block(current_running, &conds[cond_idx].block_queue);
    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx) {
    logging(LOG_INFO, "locking", "%d.%s.%d signal condition[%d]\n",
            current_running->pid, current_running->name, current_running->tid, cond_idx);
    if (list_is_empty(&conds[cond_idx].block_queue)) return ;
    do_unblock(&conds[cond_idx].block_queue);
}

void do_condition_broadcast(int cond_idx) {
    logging(LOG_INFO, "locking", "%d.%s.%d broadcast condition[%d]\n",
            current_running->pid, current_running->name, current_running->tid, cond_idx);
    while (!list_is_empty(&conds[cond_idx].block_queue))
        do_unblock(&conds[cond_idx].block_queue);
}

void do_condition_destroy(int cond_idx) {
    logging(LOG_INFO, "locking", "%d.%s.%d destroy condition[%d]\n",
            current_running->pid, current_running->name, current_running->tid, cond_idx);
    list_init(&conds[cond_idx].block_queue);
    conds[cond_idx].allocated = 0;
}
