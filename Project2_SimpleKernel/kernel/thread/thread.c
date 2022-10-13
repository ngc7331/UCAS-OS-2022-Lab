#include <csr.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/thread.h>
#include <printk.h>

extern void ret_from_exception();
extern int pcb_n;
extern void pcb_enqueue(list_node_t *queue, pcb_t *pcb);

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, regs_context_t regs) {
     /* initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    for (int i=0; i<32; i++)
        pt_regs->regs[i] = regs.regs[i];
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;
    pt_regs->sstatus = SR_SPIE;

    pt_regs->sepc = entry_point;
    pt_regs->regs[2] = user_stack;
    pt_regs->regs[4] = (reg_t) pcb;

    // set sp to simulate just returning from switch_to
    /* NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    pcb->kernel_sp = (reg_t) pt_switchto;
    pcb->user_sp = user_stack;

    // save regs to kernel_stack
    pt_switchto->regs[0] = (reg_t) ret_from_exception;
    pt_switchto->regs[1] = user_stack;
    for (int i=2; i<14; i++)
        pt_switchto->regs[i] = 0;
}

static int newtid() {
    if (current_running->type == TYPE_PROCESS) {
        return ++current_running->tid;
    }
    for (int i=0; i<=pcb_n; i++) {
        if (pcb[i].pid == current_running->pid && pcb[i].type == TYPE_PROCESS)
            return ++pcb[i].tid;
    }
    return 0;
}

pid_t thread_create(uint64_t entrypoint, void *arg) {
    pcb_t new;
    regs_context_t regs;
    regs.regs[10] = (reg_t) arg;
    new.kernel_sp = allocKernelPage(1) + PAGE_SIZE;
    new.user_sp = allocUserPage(1) + PAGE_SIZE;
    new.pid = current_running->pid;
    new.tid = newtid();
    new.type = TYPE_THREAD;
    strcpy(new.name, current_running->name);
    new.cursor_x = current_running->cursor_x;
    new.cursor_y = current_running->cursor_y;
    new.status = TASK_READY;

    init_pcb_stack(new.kernel_sp, new.user_sp, entrypoint, &new, regs);

    logging(LOG_INFO, "thread", "%d.%s created new thread, tid=%d\n", new.pid, new.name, new.tid);
    logging(LOG_DEBUG, "thread", "...entrypoint=%ld, arg=%ld\n", entrypoint, (uint64_t) arg);

    pcb[pcb_n] = new;
    pcb_enqueue(&ready_queue, &pcb[pcb_n++]);
    return new.tid;
}

void thread_join(pid_t tid, void **retval) {
    pcb_t *sub;
    for (int i=0; i<pcb_n; i++) {
        if (pcb[i].pid == current_running->pid && pcb[i].type == TYPE_THREAD && pcb[i].tid == tid)
            sub = &pcb[i];
    }
    /* FIXME:
     * for now, just do_scheduler() until sub is exited
     * is that possible to block current_running and unblock it when sub calls thread_exit()?
    */
    logging(LOG_INFO, "thread", "%d.%s waiting for %d\n", current_running->pid, current_running->name, tid);
    while (sub->status != TASK_EXITED) {
        do_scheduler();
    }

    logging(LOG_INFO, "thread", "%d.%s get %d's retval=%ld\n", current_running->pid, current_running->name, tid, sub->retval);
    *retval = sub->retval;
}

void thread_exit(void *retval) {
    logging(LOG_INFO, "thread", "%d.%s.%d exited, retval=%ld\n", current_running->pid, current_running->name, current_running->tid, (long) retval);
    current_running->retval = retval;
    current_running->status = TASK_EXITED;
    // FIXME: garbage collecter?
    do_scheduler();
}
