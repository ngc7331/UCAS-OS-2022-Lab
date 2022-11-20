#include <csr.h>
#include <os/irq.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/smp.h>
#include <os/pthread.h>
#include <printk.h>

extern void ret_from_exception();
extern void pcb_enqueue(list_node_t *queue, pcb_t *pcb);

pcb_t *get_parent(pid_t pid) {
    for (int i=0; i<NUM_MAX_TASK; i++) {
        if (pcb[i].status == TASK_UNUSED)
            break;
        if (pcb[i].pid == pid && pcb[i].type == TYPE_PROCESS)
            return &pcb[i];
    }
    return NULL;
}

static void init_tcb_stack(ptr_t kernel_stack, ptr_t entry_point, void *arg, pcb_t *tcb) {
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    for (int i=0; i<32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;
    pt_regs->sstatus = SR_SPIE | SR_SUM;
    pt_regs->sepc = entry_point;
    pt_regs->regs[10] = (reg_t) arg;

    // set sp to simulate just returning from switch_to
    /* NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    tcb->kernel_sp = (reg_t) pt_switchto;

    // save regs to kernel_stack
    pt_switchto->regs[0] = (reg_t) ret_from_exception;
    for (int i=1; i<14; i++)
        pt_switchto->regs[i] = 0;
}

pid_t pthread_create(uint64_t entrypoint, void *arg) {
    int cid = get_current_cpu_id();
    logging(LOG_INFO, "scheduler", "%d.%s.%d create thread entrypoint=0x%x%x, arg=0x%x%x\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, entrypoint>>32, entrypoint, (uint64_t)arg>>32, (uint64_t)arg);

    do_garbage_collector();
    // get new idx
    int idx = new_pcb_idx();
    if (idx < 0 || idx >= NUM_MAX_TASK) {
        logging(LOG_ERROR, "scheduler", "max task num exceeded\n");
        return 0;
    }

    // find parent
    pcb_t *parent = get_parent(current_running[cid]->pid);

    // allocate a new pgdir and copy from kernel
    pcb[idx].pgdir = parent->pgdir;

    // allocate a new page for kernel stack, set user stack
    page_t *tmp = alloc_page1();
    list_insert(&parent->page_list, &tmp->list);
    pcb[idx].kernel_sp = pcb[idx].kernel_stack_base = tmp->kva + PAGE_SIZE;
    pcb[idx].user_sp = pcb[idx].user_stack_base = USER_STACK_ADDR + (parent->tid + 1) * PAGE_SIZE * 16;

    // identifier
    pcb[idx].pid = parent->pid;
    pcb[idx].tid = parent->tid++;
    pcb[idx].type = TYPE_THREAD;
    strcpy(pcb[idx].name, parent->name);

    // cpu
    pcb[idx].cid = 0;
    pcb[idx].mask = current_running[cid]->mask;

    // screen
    pcb[idx].cursor_x = pcb[idx].cursor_y = 0;

    // status
    pcb[idx].status = TASK_READY;

    logging(LOG_INFO, "scheduler", "create %s as tid=%d\n", pcb[idx].name, pcb[idx].tid);

    init_tcb_stack(pcb[idx].kernel_sp, entrypoint, arg, &pcb[idx]);

    list_init(&pcb[idx].wait_list);
    pcb_enqueue(&ready_queue, &pcb[idx]);
    return pcb[idx].tid;
}

int pthread_join(pid_t tid) {
    int cid = get_current_cpu_id();
    int retval = 0;
    logging(LOG_INFO, "scheduler", "%d.%s.%d join tid=%d\n",
            current_running[cid]->pid, current_running[cid]->name, tid);
    for (int i=0; i<NUM_MAX_TASK; i++) {
        if (pcb[i].status == TASK_UNUSED)
            break;
        if (pcb[i].pid == current_running[cid]->pid && pcb[i].tid == tid && pcb[i].type == TYPE_THREAD) {
            if (pcb[i].status != TASK_EXITED)
                do_block(current_running[cid], &pcb[i].wait_list);
            retval = tid;
            break;
        }
    }
    return retval;
}

void pthread_exit() {
    int cid = get_current_cpu_id();
    if (current_running[cid]->type != TYPE_THREAD) // only thread can call this func
        return;
    while (!list_is_empty(&current_running[cid]->wait_list)) {
        do_unblock(&current_running[cid]->wait_list);
    }
    // forced release all locks, this will do nothing if proc doesnt hold any lock
    do_mutex_lock_release_f(current_running[cid]->pid, current_running[cid]->tid);
    // barrier & mbox will not be released by kernel
    // do kill
    current_running[cid]->status = TASK_EXITED;
    // remove pcb from any queue, this will do nothing if pcb is not in a queue
    list_delete(&current_running[cid]->list);
    // log
    logging(LOG_INFO, "scheduler", "thread %d.%s.%d exited\n", current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid);
    // never returns
    do_scheduler();
}
