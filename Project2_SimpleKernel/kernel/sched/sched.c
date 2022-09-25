#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .name = "init"
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
void pcb_enqueue(list_node_t *queue, pcb_t *pcb);
pcb_t *pcb_dequeue(list_node_t *queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    // no more tasks
    if (list_is_empty(&ready_queue))
        return ;

    pcb_t *next = pcb_dequeue(&ready_queue);
    pcb_t *prev = current_running;

    printl("[scheduler] %d.%s -> %d.%s\n", prev->pid, prev->name, next->pid, next->name);

    if (prev->status == TASK_RUNNING) {
        prev->status = TASK_READY;
        pcb_enqueue(&ready_queue, prev);
    }
    next->status = TASK_RUNNING;

    // Modify the current_running pointer.
    process_id = prev->pid;
    current_running = next;

    // switch_to current_running
    switch_to(prev, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
}

void do_block(pcb_t *pcb, list_head *queue)
{
    // block the pcb task into the block queue
    printl("[scheduler] block %d.%s\n", pcb->pid, pcb->name);
    pcb_enqueue(queue, pcb);
    pcb->status = TASK_BLOCKED;
    do_scheduler();
}

void do_unblock(list_head *queue)
{
    // unblock the `pcb` from the block queue
    pcb_t *pcb = pcb_dequeue(queue);
    printl("[scheduler] unblock %d.%s\n", pcb->pid, pcb->name);
    pcb->status = TASK_READY;
    pcb_enqueue(&ready_queue, pcb);
}
