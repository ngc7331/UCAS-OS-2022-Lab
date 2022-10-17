#include <os/list.h>
#include <os/sched.h>
#include <type.h>
#include <printk.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

void pcb_enqueue(list_node_t *queue, pcb_t *pcb);

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // Pick out tasks that should wake up from the sleep queue
    for (list_node_t *p=sleep_queue.next; p!=&sleep_queue; ) {
        pcb_t *pcb = list_entry(p, pcb_t, list);
        if (pcb->wakeup_time <= get_ticks()) {
            logging(LOG_INFO, "timer", "unblock %d.%s, expected at %d\n", pcb->pid, pcb->name, pcb->wakeup_time);
            p = list_delete(p);
            pcb->status = TASK_READY;
            pcb_enqueue(&ready_queue, pcb);
        } else {
            p = p->next;
        }
    }
}
