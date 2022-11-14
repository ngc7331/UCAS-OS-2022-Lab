#include <csr.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <printk.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack[2] = {
    INIT_KERNEL_STACK + PAGE_SIZE,
    INIT_KERNEL_STACK + 2 * PAGE_SIZE,
};
pcb_t pid0_pcb[2];

// last allocated pid
int pid_n = 0;
int pcb_n = 0;

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running[2];

/* global process id */
pid_t process_id = 1;

extern void ret_from_exception();
extern void init_shell();

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, int argc,
#ifdef S_CORE_P3
    uint64_t arg0, uint64_t arg1, uint64_t arg2
#else
    char *argv[]
#endif
    ) {
     /* initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    char *user_sp = (char *) user_stack;

    for (int i=0; i<32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;
    pt_regs->sstatus = SR_SPIE | SR_SUM;
    pt_regs->sepc = entry_point;
    pt_regs->regs[10] = argc;
#ifdef S_CORE_P3
    pt_regs->regs[11] = arg0;
    pt_regs->regs[12] = arg1;
    pt_regs->regs[13] = arg2;
    logging(LOG_DEBUG, "init", "... arg0=%ld, arg1=%ld, arg2=%ld\n", arg0, arg1, arg2);
#else
    char **pt_argv = (char **) (user_stack - (argc + 1) * 8);
    user_sp = (char *) pt_argv;
    for (int i=0; i<argc; i++) {
        user_sp -= strlen(argv[i]) + 1;
        strcpy(user_sp, argv[i]);
        pt_argv[i] = user_sp;
        logging(LOG_DEBUG, "init", "... argv[%d]=\"%s\" placed at %x\n", i, pt_argv[i], user_sp);
    }
    pt_argv[argc] = NULL;
    pt_regs->regs[11] = (reg_t) pt_argv;
    // alignment
    user_sp = (char *) ROUNDDOWN(user_sp, 16);
#endif

    // set sp to simulate just returning from switch_to
    /* NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    pcb->kernel_sp = (reg_t) pt_switchto;
    pcb->user_sp = (reg_t) user_sp;
    logging(LOG_DEBUG, "init", "... kernel_sp=0x%x%x, user_sp=0x%lx\n", pcb->kernel_sp >> 32, pcb->kernel_sp, pcb->user_sp);

    // save regs to kernel_stack
    pt_switchto->regs[0] = (reg_t) ret_from_exception;
    for (int i=1; i<14; i++)
        pt_switchto->regs[i] = 0;
}

void pcb_enqueue(list_node_t *queue, pcb_t *pcb) {
    list_insert(queue->prev, &pcb->list);
}

pcb_t *pcb_dequeue(list_node_t *queue, unsigned cid) {
    for (list_node_t *p=queue->next; p!=queue; p=p->next) {
        pcb_t *pcb = list_entry(p, pcb_t, list);
        if (pcb->mask & cid) {
            list_delete(p);
            return pcb;
        }
    }
    return NULL;
}

#ifdef S_CORE_P3
pid_t init_pcb(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
#else
pid_t init_pcb(char *name, int argc, char *argv[]) {
    int id = get_taskid_by_name(name, APP);
#endif
    if (id < 0 || id >= appnum)
        return 0;

    int cid = get_current_cpu_id();

    // allocate a new pgdir and copy from kernel
    pcb[pcb_n].pgdir = allocPage(1);
    share_pgtable(pcb[pcb_n].pgdir, pid0_pcb[cid].pgdir);

    // allocate new pages and load task to it
    // if S_CORE, alloc a large page; else, alloc first normal page
    uintptr_t page = alloc_page_helper(apps[id].entrypoint, pcb[pcb_n].pgdir);
#ifndef S_CORE
    // and alloc remaining normal pages
    for (uint64_t i=PAGE_SIZE; i<apps[id].memsize; i+=PAGE_SIZE) {
        alloc_page_helper(apps[id].entrypoint + i, pcb[pcb_n].pgdir);
    }
#endif
    load_img(page, apps[id].phyaddr, apps[id].size, 1);

    // allocate a new page for kernel stack, set user stack
    pcb[pcb_n].kernel_sp = pcb[pcb_n].kernel_stack_base = allocPage(1) + PAGE_SIZE;
#ifndef S_CORE
    alloc_page_helper(USER_STACK_ADDR - PAGE_SIZE, pcb[pcb_n].pgdir) + PAGE_SIZE;
#endif
    pcb[pcb_n].user_sp = pcb[pcb_n].user_stack_base = USER_STACK_ADDR;

    // identifier
    pcb[pcb_n].pid = ++pid_n;
    pcb[pcb_n].tid = 0;
    pcb[pcb_n].type = TYPE_PROCESS;
    strcpy(pcb[pcb_n].name, apps[id].name);

    // cpu
    pcb[pcb_n].cid = 0;
    pcb[pcb_n].mask = current_running[cid]->mask;

    // screen
    pcb[pcb_n].cursor_x = pcb[pcb_n].cursor_y = 0;

    // status
    pcb[pcb_n].status = TASK_READY;

    // FIXME: pthread
    pcb[pcb_n].retval = NULL;
    pcb[pcb_n].joined = NULL;

    logging(LOG_INFO, "init", "load %s as pid=%d\n", pcb[pcb_n].name, pcb[pcb_n].pid);
    logging(LOG_DEBUG, "init", "... pgdir=0x%x%x, page=0x%x%x\n", pcb[pcb_n].pgdir >> 32, pcb[pcb_n].pgdir, page >> 32, page);
    logging(LOG_DEBUG, "init", "... entrypoint=0x%lx\n", apps[id].entrypoint);

    init_pcb_stack(pcb[pcb_n].kernel_sp, pcb[pcb_n].user_sp, apps[id].entrypoint, &pcb[pcb_n], argc,
#ifdef S_CORE_P3
    arg0, arg1, arg2
#else
    argv
#endif
    );

    list_init(&pcb[pcb_n].wait_list);
    pcb_enqueue(&ready_queue, &pcb[pcb_n]);
    return pcb[pcb_n++].pid;
}

void do_scheduler(void) {
    int cid = get_current_cpu_id();

    // Check sleep queue to wake up PCBs
    check_sleeping();

    pcb_t *prev = current_running[cid];
    pcb_t *next = pcb_dequeue(&ready_queue, 1 << cid);
    if (next == NULL) {
        if (current_running[cid]->status == TASK_RUNNING) {
            logging(LOG_VERBOSE, "scheduler", "ready_queue empty, back to %d.%s.%d\n", prev->pid, prev->name, prev->tid);
            return ;
        } else if (pid0_pcb[0].status == TASK_READY) {
            logging(LOG_VERBOSE, "scheduler", "ready_queue empty, use 0.init.0\n");
            next = &pid0_pcb[0];
        } else if (pid0_pcb[1].status == TASK_READY) {
            logging(LOG_VERBOSE, "scheduler", "ready_queue empty, use 0.init.1\n");
            next = &pid0_pcb[1];
        } else{
            logging(LOG_CRITICAL, "scheduler", "ready_queue empty, kernel not ready yet\n");
            assert(0);
        }
    }

    logging(LOG_VERBOSE, "scheduler", "%d.%s.%d -> %d.%s.%d\n", prev->pid, prev->name, prev->tid, next->pid, next->name, next->tid);

    if (prev->status == TASK_RUNNING) {
        prev->status = TASK_READY;
        if (prev->pid != 0)
            pcb_enqueue(&ready_queue, prev);
    }
    next->status = TASK_RUNNING;
    next->cid = cid;

    // Modify the current_running pointer.
    process_id = prev->pid;
    current_running[cid] = next;

    // switch pagedir
    set_satp(SATP_MODE_SV39, next->pid, kva2pa(next->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();

    // switch_to current_running
    switch_to(prev, current_running[cid]);
}

void do_sleep(uint32_t sleep_time) {
    int cid = get_current_cpu_id();
    // sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // set the wake up time for the blocked task
    current_running[cid]->wakeup_time = get_ticks() + sleep_time * time_base;
    logging(LOG_INFO, "timer", "set wakeup time %d for %d.%s.%d\n",
            current_running[cid]->wakeup_time, current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid);
    // call do_block
    do_block(current_running[cid], &sleep_queue);
}

void do_block(pcb_t *pcb, list_head *queue) {
    // block the pcb task into the block queue
    logging(LOG_INFO, "scheduler", "block %d.%s.%d\n", pcb->pid, pcb->name, pcb->tid);
    pcb_enqueue(queue, pcb);
    pcb->status = TASK_BLOCKED;
    do_scheduler();
}

void do_unblock(list_head *queue) {
    // unblock the `pcb` from the block queue
    pcb_t *pcb = pcb_dequeue(queue, 0xFFFF);
    if (pcb == NULL) {
        logging(LOG_ERROR, "scheduler", "failed to unblock from queue %x\n", queue);
        return ;
    }
    logging(LOG_INFO, "scheduler", "unblock %d.%s.%d\n", pcb->pid, pcb->name, pcb->tid);
    pcb->status = TASK_READY;
    pcb_enqueue(&ready_queue, pcb);
}

#ifdef S_CORE_P3
pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    int cid = get_current_cpu_id();
    logging(LOG_INFO, "scheduler", "%d.%s.%d exec id=%d, argc=%d, arg0=%ld, arg1=%ld, arg2=%ld\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, id, argc, arg0, arg1, arg2);
    return init_pcb(id, argc, arg0, arg1, arg2);
}
#else
pid_t do_exec(char *name, int argc, char *argv[]) {
    int cid = get_current_cpu_id();
    logging(LOG_INFO, "scheduler", "%d.%s.%d exec name=%s, argc=%d, argv=%x\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, name, argc, argv);
    return init_pcb(name, argc, argv);
}
#endif

void do_exit(void) {
    int cid = get_current_cpu_id();
    do_kill(current_running[cid]->pid);
    do_scheduler();
}

int do_kill(pid_t pid) {
    int cid = get_current_cpu_id();
    if (pid == 0) {
        logging(LOG_ERROR, "scheduler", "trying to kill init, abort\n");
        return -1;
    }
    logging(LOG_INFO, "scheduler", "%d.%s.%d kill %d\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, pid);
    int retval = 0;
    // kill process and its threads
    for (int i=0; i<pcb_n; i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
            // wakeup waiting processes
            while (!list_is_empty(&pcb[i].wait_list)) {
                do_unblock(&pcb[i].wait_list);
            }
            // forced release all locks, this will do nothing if proc doesnt hold any lock
            do_mutex_lock_release_f(pcb[i].pid);
            // barrier & mbox will not be released by kernel
            // do kill
            pcb[i].status = TASK_EXITED;
            // remove pcb from any queue, this will do nothing if pcb is not in a queue
            list_delete(&pcb[i].list);
            // return success
            retval = 1;
            // log
            logging(LOG_INFO, "scheduler", "%d.%s.%d %s\n",
                    pcb[i].pid, pcb[i].name, pcb[i].tid, pid==current_running[cid]->pid ? "exited" : "is killed");
            // shell is killed, warn and restart
            if (strcmp("shell", pcb[i].name) == 0) {
                init_shell();
                logging(LOG_WARNING, "scheduler", "shell is killed and restarted\n");
            }
        }
    }
    // FIXME: garbage collector?
    return retval;
}

int do_waitpid(pid_t pid) {
    int cid = get_current_cpu_id();
    int retval = 0;
    logging(LOG_INFO, "scheduler", "%d.%s wait %d\n",
            current_running[cid]->pid, current_running[cid]->name, pid);
    for (int i=0; i<pcb_n; i++) {
        if (pcb[i].pid == pid && pcb[i].type == TYPE_PROCESS) {
            if (pcb[i].status != TASK_EXITED)
                do_block(current_running[cid], &pcb[i].wait_list);
            retval = pid;
            break;
        }
    }
    return retval;
}

void do_process_show(int mode) {
    char *status_dict[] = {
        "BLOCK  ",
        "RUNNING",
        "READY  ",
        "EXITED "
    };
    printk("--------------------- PROCESS TABLE START ---------------------\n");
    printk("| idx | PID | TID | name             | status  | cpu |  mask  |\n");
    for (int i=0; i<pcb_n; i++) {
        if (mode == 0 && pcb[i].status == TASK_EXITED)
            continue;
        char buf[17] = "                ";
        // collapse name longer than 15
        int len = strlen(pcb[i].name);
        strncpy(buf, pcb[i].name, len<16 ? len : 16);
        if (len > 16)
            buf[13] = buf[14] = buf[15] = '.';
        printk("| %03d | %03d | %03d | %s | %s |  %c  | 0x%04x |\n",
               i, pcb[i].pid, pcb[i].tid, buf, status_dict[pcb[i].status],
               pcb[i].status == TASK_RUNNING ? pcb[i].cid + '0' : '-', pcb[i].mask);
    }
    printk("---------------------- PROCESS TABLE END ----------------------\n");
}

pid_t do_getpid(void) {
    int cid = get_current_cpu_id();
    return current_running[cid]->pid;
}

void do_taskset(pid_t pid, unsigned mask) {
    for (int i=0; i<pcb_n; i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED)
            pcb[i].mask = mask;
    }
}
