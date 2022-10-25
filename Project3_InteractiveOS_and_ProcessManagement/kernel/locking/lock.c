#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/irq.h>
#include <atomic.h>
#include <printk.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    // initialize mlocks
    for (int i=0; i<LOCK_NUM; i++) {
        spin_lock_init(&mlocks[i].lock);
        list_init(&mlocks[i].block_queue);
        mlocks[i].allocated = 0;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    // initialize spin lock
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    // try to acquire spin lock
    int status = atomic_swap_d(LOCKED, (ptr_t)&lock->status);
    return status;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    // acquire spin lock
    while (spin_lock_try_acquire(lock) == LOCKED) ;
}

void spin_lock_release(spin_lock_t *lock)
{
    // release spin lock
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    // disable_preempt();
    // initialize mutex lock
    // key has been allocated with a lock
    for (int i=0; i<LOCK_NUM; i++) {
        if (mlocks[i].key == key) {
            mlocks[i].allocated ++;
            logging(LOG_INFO, "mlock", "key=%d, lock#%d found for %d.%s\n", key, i, current_running->pid, current_running->name);
            // enable_preempt();
            return i;
        }
    }
    // allocate a new lock
    for (int i=0; i<LOCK_NUM; i++) {
        if (mlocks[i].allocated == 0) {
            mlocks[i].allocated = 1;
            mlocks[i].key = key;
            logging(LOG_INFO, "mlock", "key=%d, lock#%d allocated for %d.%s\n", key, i, current_running->pid, current_running->name);
            // enable_preempt();
            return i;
        }
    }
    // allocate failed
    // enable_preempt();
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    // disable_preempt();
    // acquire mutex lock
    if (atomic_swap_d(LOCKED, (ptr_t)&mlocks[mlock_idx].lock.status) == UNLOCKED) {
        logging(LOG_INFO, "mlock", "%d.%s acquire lock#%d successfully\n", current_running->pid, current_running->name, mlock_idx);
        // record pid
        mlocks[mlock_idx].pid = current_running->pid;
        // enable_preempt();
    } else {
        logging(LOG_INFO, "mlock", "%d.%s acquire lock#%d failed, block\n", current_running->pid, current_running->name, mlock_idx);
        // enable_preempt();
        do_block(current_running, &mlocks[mlock_idx].block_queue);
        // unblocked, record pid
        mlocks[mlock_idx].pid = current_running->pid;
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    // disable_preempt();
    // release mutex lock
    if (list_is_empty(&mlocks[mlock_idx].block_queue)) {
        logging(LOG_INFO, "mlock", "%d.%s release lock#%d successfully\n", current_running->pid, current_running->name, mlock_idx);
        mlocks[mlock_idx].lock.status = UNLOCKED;
        mlocks[mlock_idx].pid = 0;
    } else {
        logging(LOG_INFO, "mlock", "%d.%s release lock#%d successfully, unblock a process from queue\n", current_running->pid, current_running->name, mlock_idx);
        // no need to modify mlock.pid, since the unblocked proc will modify it
        do_unblock(&mlocks[mlock_idx].block_queue);
    }
    // enable_preempt();
}

void do_mutex_lock_release_f(pid_t pid) {
    // forced release all locks held by proc
    // NOTE: should be called BY do_kill() ONLY at this moment
    for (int i=0; i<LOCK_NUM; i++) {
        if (mlocks[i].pid == pid)
            do_mutex_lock_release(i);
    }
}
