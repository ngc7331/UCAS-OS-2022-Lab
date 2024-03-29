#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/list.h>
#include <os/irq.h>
#include <atomic.h>
#include <printk.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void) {
    // initialize mlocks
    for (int i=0; i<LOCK_NUM; i++) {
        spin_lock_init(&mlocks[i].lock);
        list_init(&mlocks[i].block_queue);
        mlocks[i].allocated = 0;
    }
}

void spin_lock_init(spin_lock_t *lock) {
    // initialize spin lock
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock) {
    // try to acquire spin lock
    int status = atomic_swap_d(LOCKED, (ptr_t)&lock->status);
    return status;
}

void spin_lock_acquire(spin_lock_t *lock) {
    // acquire spin lock
    while (spin_lock_try_acquire(lock) == LOCKED) ;
}

void spin_lock_release(spin_lock_t *lock) {
    // release spin lock
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key) {
    int cid = get_current_cpu_id();
    // disable_preempt();
    // initialize mutex lock
    int idx = -1;
    // key has been allocated with a lock
    for (int i=0; i<LOCK_NUM; i++) {
        if (mlocks[i].key == key) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        // allocate a new lock
        for (int i=0; i<LOCK_NUM; i++) {
            if (mlocks[i].allocated == 0) {
                idx = i;
                break;
            }
        }
    }
    if (idx >= 0) {
        // allocate success
        mlocks[idx].allocated ++;
        mlocks[idx].key = key;
        logging(LOG_INFO, "locking", "%d.%s.%d get mlock[%d] with key=%d\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, idx, key);
    } else {
        // allocate failed
        logging(LOG_WARNING, "locking", "%d.%s.%d init mlock failed\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid);
    }
    // allocate failed
    // enable_preempt();
    return idx;
}

void do_mutex_lock_acquire(int mlock_idx) {
    int cid = get_current_cpu_id();
    // disable_preempt();
    // acquire mutex lock
    if (atomic_swap_d(LOCKED, (ptr_t)&mlocks[mlock_idx].lock.status) == UNLOCKED) {
        logging(LOG_INFO, "locking", "%d.%s.%d acquire mlock[%d] successfully\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, mlock_idx);
        // enable_preempt();
    } else {
        logging(LOG_INFO, "locking", "%d.%s.%d acquire mlock[%d] failed, block\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, mlock_idx);
        // enable_preempt();
        do_block(current_running[cid], &mlocks[mlock_idx].block_queue);
    }
    // record pid
    mlocks[mlock_idx].pid = current_running[cid]->pid;
    mlocks[mlock_idx].tid = current_running[cid]->tid;
}

void do_mutex_lock_release(int mlock_idx) {
    int cid = get_current_cpu_id();
    // disable_preempt();
    // release mutex lock
    if (list_is_empty(&mlocks[mlock_idx].block_queue)) {
        logging(LOG_INFO, "locking", "%d.%s.%d release mlock[%d] successfully\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, mlock_idx);
        mlocks[mlock_idx].lock.status = UNLOCKED;
    } else {
        logging(LOG_INFO, "locking", "%d.%s.%d release mlock[%d] successfully, unblock a process from queue\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, mlock_idx);
        do_unblock(&mlocks[mlock_idx].block_queue);
    }
    // enable_preempt();
    mlocks[mlock_idx].pid = 0;
    mlocks[mlock_idx].tid = 0;
}

void do_mutex_lock_release_f(pid_t pid, pid_t tid) {
    int cid = get_current_cpu_id();
    // forced release all locks held by proc
    // NOTE: should be called BY do_kill() ONLY at this moment
    if (pid != current_running[cid]->pid)
        logging(LOG_WARNING, "locking", "%d.%s.%d forced release all mlocks held by pid=%d, tid=%d\n",
                current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, pid, tid);
    for (int i=0; i<LOCK_NUM; i++) {
        if (mlocks[i].pid == pid && mlocks[i].tid == tid)
            do_mutex_lock_release(i);
    }
}
