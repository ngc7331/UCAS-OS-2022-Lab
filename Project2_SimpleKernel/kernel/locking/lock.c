#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
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
    // initialize mutex lock
    // key has been allocated with a lock
    for (int i=0; i<LOCK_NUM; i++) {
        if (mlocks[i].key == key) {
            mlocks[i].allocated ++;
            printl("[mlock] lock#%d found, key=%d\n", i, key);
            return i;
        }
    }
    // allocate a new lock
    for (int i=0; i<LOCK_NUM; i++) {
        if (mlocks[i].allocated == 0) {
            mlocks[i].allocated = 1;
            mlocks[i].key = key;
            printl("[mlock] lock#%d inited, key=%d\n", i, key);
            return i;
        }
    }
    // allocate failed
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    // acquire mutex lock
    printl("[mlock] acquire lock#%d... ", mlock_idx);
    if (atomic_swap_d(LOCKED, (ptr_t)&mlocks[mlock_idx].lock.status) == UNLOCKED) {
        printl("done\n");
    } else {
        printl("locked, block\n");
        do_block(current_running, &mlocks[mlock_idx].block_queue);
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    // release mutex lock
    printl("[mlock] release lock#%d... ", mlock_idx);
    if (list_is_empty(&mlocks[mlock_idx].block_queue)) {
        printl("done, block_queue empty\n");
        mlocks[mlock_idx].lock.status = UNLOCKED;
    } else {
        printl("done, unblock\n");
        do_unblock(&mlocks[mlock_idx].block_queue);
    }
}
