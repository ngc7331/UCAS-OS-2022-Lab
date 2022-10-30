#include <os/lock.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/irq.h>
#include <atomic.h>
#include <printk.h>

mailbox_t mboxes[MBOX_NUM];

static void _do_mutex_lock_acquire(mutex_lock_t *mlock) {
    // acquire mutex lock
    if (atomic_swap_d(LOCKED, (ptr_t)(&(mlock->lock.status))) == UNLOCKED) {
    } else {
        do_block(current_running, &mlock->block_queue);
    }
}

static void _do_mutex_lock_release(mutex_lock_t *mlock) {
    // release mutex lock
    if (list_is_empty(&mlock->block_queue)) {
        mlock->lock.status = UNLOCKED;
    } else {
        do_unblock(&mlock->block_queue);
    }
}

static void _do_condition_wait(condition_t *cond, mutex_lock_t *mlock) {
    _do_mutex_lock_release(mlock);
    do_block(current_running, &cond->block_queue);
    _do_mutex_lock_acquire(mlock);
}

static void _do_condition_signal(condition_t *cond) {
    if (list_is_empty(&cond->block_queue)) return ;
    do_unblock(&cond->block_queue);
}

void init_mbox(void) {
    for (int i=0; i<MBOX_NUM; i++) {
        mboxes[i].size = 0;
        mboxes[i].rp = 0;
        mboxes[i].allocated = 0;
        // init lock
        mboxes[i].lock.lock.status = UNLOCKED;
        list_init(&mboxes[i].lock.block_queue);
        // init cond
        list_init(&mboxes[i].full.block_queue);
        list_init(&mboxes[i].empty.block_queue);
    }
}

int do_mbox_open(char *name) {
    // disable_preempt();
    // open a mailbox
    int idx = -1;
    // mailbox *name exists
    for (int i=0; i<MBOX_NUM; i++) {
        if (strcmp(mboxes[i].name, name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        // allocate a new mailbox
        for (int i=0; i<BARRIER_NUM; i++) {
            if (mboxes[i].allocated == 0) {
                idx = i;
                break;
            }
        }
    }
    if (idx >= 0) {
        // allocate success
        mboxes[idx].allocated ++;
        strcpy(mboxes[idx].name, name);
        logging(LOG_INFO, "locking", "%d.%s.%d open mailbox[%d] %s\n",
                current_running->pid, current_running->name, current_running->tid, idx, name);
    } else {
        // allocate failed
        logging(LOG_WARNING, "locking", "%d.%s.%d open mailbox failed\n",
                current_running->pid, current_running->name, current_running->tid);
    }
    // enable_preempt();
    return idx;
}

void do_mbox_close(int mbox_idx) {
    logging(LOG_INFO, "locking", "%d.%s.%d close mailbox[%d] %s\n",
            current_running->pid, current_running->name, current_running->tid, mbox_idx, mboxes[mbox_idx].name);
    if (--mboxes[mbox_idx].allocated == 0) {
        mboxes[mbox_idx].size = 0;
        mboxes[mbox_idx].rp = 0;
        // init lock
        mboxes[mbox_idx].lock.lock.status = UNLOCKED;
        list_init(&mboxes[mbox_idx].lock.block_queue);
        // init cond
        list_init(&mboxes[mbox_idx].full.block_queue);
        list_init(&mboxes[mbox_idx].empty.block_queue);
    }
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length) {
    _do_mutex_lock_acquire(&mboxes[mbox_idx].lock);
    logging(LOG_INFO, "locking", "%d.%s.%d send %d bytes to mailbox[%d] %s\n",
            current_running->pid, current_running->name, current_running->tid, msg_length, mbox_idx, mboxes[mbox_idx].name);
    // wait until msgbox is available
    while (MAX_MBOX_LENGTH - mboxes[mbox_idx].size - msg_length < 0) {
        _do_condition_wait(&mboxes[mbox_idx].full, &mboxes[mbox_idx].lock);
    }
    // send
    int wp = (mboxes[mbox_idx].rp + mboxes[mbox_idx].size) % MAX_MBOX_LENGTH;
    for (int i=0; i<msg_length; i++, wp = (wp + 1) % MAX_MBOX_LENGTH)
        mboxes[mbox_idx].buf[wp] = ((char *) msg)[i];
    mboxes[mbox_idx].size += msg_length;

    logging(LOG_INFO, "locking", "%d.%s.%d send %d bytes to mailbox[%d] %s + %d, success\n",
            current_running->pid, current_running->name, current_running->tid, msg_length, mbox_idx, mboxes[mbox_idx].name, mboxes[mbox_idx].size);
    _do_condition_signal(&mboxes[mbox_idx].empty);
    _do_mutex_lock_release(&mboxes[mbox_idx].lock);
    return 0;
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length) {
    _do_mutex_lock_acquire(&mboxes[mbox_idx].lock);
    logging(LOG_INFO, "locking", "%d.%s.%d recv %d bytes from mailbox[%d] %s\n",
            current_running->pid, current_running->name, current_running->tid, msg_length, mbox_idx, mboxes[mbox_idx].name);
    // wait until msgbox is available
    while (mboxes[mbox_idx].size < msg_length) {
        _do_condition_wait(&mboxes[mbox_idx].empty, &mboxes[mbox_idx].lock);
    }
    // recv
    for (int i=0; i<msg_length; i++, mboxes[mbox_idx].rp = (mboxes[mbox_idx].rp + 1) % MAX_MBOX_LENGTH)
        ((char *) msg)[i] = mboxes[mbox_idx].buf[mboxes[mbox_idx].rp];
    mboxes[mbox_idx].size -= msg_length;

    logging(LOG_INFO, "locking", "%d.%s.%d recv %d bytes from mailbox[%d] %s success\n",
            current_running->pid, current_running->name, current_running->tid, msg_length, mbox_idx, mboxes[mbox_idx].name);
    _do_condition_signal(&mboxes[mbox_idx].full);
    _do_mutex_lock_release(&mboxes[mbox_idx].lock);
    return 0;
}
