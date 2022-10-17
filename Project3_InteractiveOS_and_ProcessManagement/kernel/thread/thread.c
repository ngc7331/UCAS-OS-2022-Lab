#include <csr.h>
#include <os/irq.h>
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
        return current_running->tid++;
    }
    for (int i=0; i<=pcb_n; i++) {
        if (pcb[i].pid == current_running->pid && pcb[i].type == TYPE_PROCESS)
            return pcb[i].tid++;
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
    new.retval = NULL;
    new.joined = NULL;

    init_pcb_stack(new.kernel_sp, new.user_sp, entrypoint, &new, regs);

    logging(LOG_INFO, "thread", "%d.%s created new thread, tid=%d\n", new.pid, new.name, new.tid);
    logging(LOG_DEBUG, "thread", "...entrypoint=%ld, arg=%ld\n", entrypoint, (uint64_t) arg);

    // disable_preempt();
    pcb[pcb_n] = new;
    pcb_enqueue(&ready_queue, &pcb[pcb_n++]);
    // enable_preempt();
    return new.tid;
}

void thread_join(pid_t tid, void **retval) {
    // find sub by tid
    pcb_t *sub = NULL;
    for (int i=0; i<pcb_n; i++) {
        if (pcb[i].pid == current_running->pid && pcb[i].type == TYPE_THREAD && pcb[i].tid == tid)
            sub = &pcb[i];
    }
    // invalid tid
    if (sub == NULL) {
        logging(LOG_CRITICAL, "thread", "%d.%s trying to join thread %d: not found\n", current_running->pid, current_running->name, tid);
        return ;
    }

    // try join
    // disable_preempt();
    if (sub->joined != NULL) {
        logging(LOG_CRITICAL, "thread", "%d.%s trying to join thread %d: not joinable\n", current_running->pid, current_running->name, tid);
        logging(LOG_DEBUG, "thread", "...sub.joined=%x\n", sub->joined);
        // enable_preempt();
        return ;
    }
    sub->joined = current_running;
    // enable_preempt();

    // wait until sub is exited
    if (sub->status != TASK_EXITED) {
        logging(LOG_INFO, "thread", "%d.%s waiting for thread %d\n", current_running->pid, current_running->name, tid);
        current_running->status = TASK_BLOCKED;
        do_scheduler();
    }

    // return
    if (retval != NULL) {
        logging(LOG_INFO, "thread", "%d.%s join thread %d complete, get retval=%ld\n", current_running->pid, current_running->name, tid, sub->retval);
        *retval = sub->retval;
    } else {
        logging(LOG_INFO, "thread", "%d.%s join thread %d complete\n", current_running->pid, current_running->name, tid);
    }
}

void thread_exit(void *retval) {
    logging(LOG_INFO, "thread", "%d.%s.%d exited, retval=%ld\n", current_running->pid, current_running->name, current_running->tid, (long) retval);
    // wake up joined task
    if (current_running->joined != NULL) {
        current_running->joined->status = TASK_READY;
        pcb_enqueue(&ready_queue, current_running->joined);
    }
    current_running->retval = retval;
    current_running->status = TASK_EXITED;

    // FIXME: garbage collecter?

    do_scheduler();
}
