#include <csr.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <printk.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .name = "init",
    .status = TASK_RUNNING
};

// last allocated pid
int pid_n = 0;
int pcb_n = 0;

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

extern void ret_from_exception();
extern void init_shell();

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, int argc,
#ifdef S_CORE
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
    pt_regs->sstatus = SR_SPIE;
    pt_regs->sepc = entry_point;
    pt_regs->regs[10] = argc;
#ifdef S_CORE
    pt_regs->regs[11] = arg0;
    pt_regs->regs[12] = arg1;
    pt_regs->regs[13] = arg2;
#else
    char **pt_argv = (char **) (user_stack - (argc + 1) * 8);
    user_sp = (char *) pt_argv;
    for (int i=0; i<argc; i++) {
        user_sp -= strlen(argv[i]) + 1;
        strcpy(user_sp, argv[i]);
        pt_argv[i] = user_sp;
    }
    pt_argv[argc] = NULL;
    pt_regs->regs[11] = (reg_t) pt_argv;
    // alignment
    user_sp = (char *) ROUNDDOWN(user_sp, 128);
#endif

    // set sp to simulate just returning from switch_to
    /* NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    pcb->kernel_sp = (reg_t) pt_switchto;
    pcb->user_sp = (reg_t) user_sp;

    // save regs to kernel_stack
    pt_switchto->regs[0] = (reg_t) ret_from_exception;
    pt_switchto->regs[1] = user_stack;
    for (int i=2; i<14; i++)
        pt_switchto->regs[i] = 0;
}

void pcb_enqueue(list_node_t *queue, pcb_t *pcb) {
    list_insert(queue->prev, &pcb->list);
}

pcb_t *pcb_dequeue(list_node_t *queue) {
    pcb_t *pcb = list_entry(queue->next, pcb_t, list);
    list_delete(queue->next);
    return pcb;
}

#ifdef S_CORE
pid_t init_pcb(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
#else
pid_t init_pcb(char *name, int argc, char *argv[]) {
    int id = get_taskid_by_name(name, APP);
#endif
    if (id < 0 || id >= appnum)
        return 0;
    load_task_img(id, APP);
    pcb[pcb_n].kernel_sp = pcb[pcb_n].kernel_stack_base = allocKernelPage(1) + PAGE_SIZE;
    pcb[pcb_n].user_sp = pcb[pcb_n].user_stack_base = allocUserPage(1) + PAGE_SIZE;
    pcb[pcb_n].pid = ++pid_n;
    pcb[pcb_n].tid = 0;
    pcb[pcb_n].type = TYPE_PROCESS;
    strcpy(pcb[pcb_n].name, apps[id].name);
    pcb[pcb_n].cursor_x = pcb[pcb_n].cursor_y = 0;
    pcb[pcb_n].status = TASK_READY;
    pcb[pcb_n].retval = NULL;
    pcb[pcb_n].joined = NULL;

    logging(LOG_INFO, "init", "load %s as pid=%d\n", pcb[pcb_n].name, pcb[pcb_n].pid);
    logging(LOG_VERBOSE, "init", "...entrypoint=%x kernel_sp=%x user_sp=%x\n", apps[id].entrypoint, pcb[pcb_n].kernel_sp, pcb[pcb_n].user_sp);

    init_pcb_stack(pcb[pcb_n].kernel_sp, pcb[pcb_n].user_sp, apps[id].entrypoint, &pcb[pcb_n], argc,
#ifdef S_CORE
    arg0, arg1, arg2
#else
    argv
#endif
    );

    list_init(&pcb[pcb_n].wait_list);
    pcb_enqueue(&ready_queue, &pcb[pcb_n]);
    return pcb[pcb_n++].pid;
}

void do_scheduler(void)
{
    // Check sleep queue to wake up PCBs
    check_sleeping();

    // no more tasks
    if (list_is_empty(&ready_queue))
        return ;

    pcb_t *next = pcb_dequeue(&ready_queue);
    pcb_t *prev = current_running;

    logging(LOG_VERBOSE, "scheduler", "%d.%s -> %d.%s\n", prev->pid, prev->name, next->pid, next->name);

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
    screen_move_cursor(current_running->cursor_x, current_running->cursor_y);
}

void do_sleep(uint32_t sleep_time)
{
    // sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // set the wake up time for the blocked task
    current_running->wakeup_time = get_ticks() + sleep_time * time_base;
    logging(LOG_INFO, "timer", "block %d.%s until %d\n", current_running->pid, current_running->name, current_running->wakeup_time);
    // call do_block
    do_block(current_running, &sleep_queue);
}

void do_block(pcb_t *pcb, list_head *queue)
{
    // block the pcb task into the block queue
    pcb_enqueue(queue, pcb);
    pcb->status = TASK_BLOCKED;
    do_scheduler();
}

void do_unblock(list_head *queue)
{
    // unblock the `pcb` from the block queue
    pcb_t *pcb = pcb_dequeue(queue);
    pcb->status = TASK_READY;
    pcb_enqueue(&ready_queue, pcb);
}

#ifdef S_CORE
pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    logging(LOG_INFO, "exec", "id=%d, argc=%d, arg0=%ld, arg1=%ld, arg2=%ld\n", id, argc, arg0, arg1, arg2);
    return init_pcb(id, argc, arg0, arg1, arg2);
}
#else
pid_t do_exec(char *name, int argc, char *argv[]) {
    logging(LOG_INFO, "exec", "name=%s, argc=%d, argv=%x\n", name, argc, argv);
    return init_pcb(name, argc, argv);
}
#endif

void do_exit(void) {
    do_kill(current_running->pid);
    do_scheduler();
}

int do_kill(pid_t pid) {
    if (pid == 0) {
        logging(LOG_CRITICAL, "scheduler", "trying to kill init, abort\n");
        return -1;
    }
    logging(LOG_INFO, "scheduler", "%d.%s kill %d\n", current_running->pid, current_running->name, pid);
    int retval = 0;
    // kill process and its threads
    for (int i=0; i<pcb_n; i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
            // wakeup waiting processes
            while (!list_is_empty(&pcb[i].wait_list)) {
                do_unblock(&pcb[i].wait_list);
                logging(LOG_INFO, "scheduler", "wakeup waiting process\n");
            }
            // FIXME: collect unreleased locks?
            // do kill
            pcb[i].status = TASK_EXITED;
            list_delete(&pcb[i].list);
            logging(LOG_INFO, "scheduler", "%d.%s.%d is killed\n", pcb[i].pid, pcb[i].name, pcb[i].tid);
            retval = 1;
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
    int retval = 0;
    logging(LOG_INFO, "scheduler", "%d.%s wait %d\n", current_running->pid, current_running->name, pid);
    for (int i=0; i<pcb_n; i++) {
        if (pcb[i].pid == pid && pcb[i].type == TYPE_PROCESS) {
            if (pcb[i].status != TASK_EXITED)
                do_block(current_running, &pcb[i].wait_list);
            retval = pid;
        }
    }
    return retval;
}

void do_process_show(void) {
    char *status_dict[] = {
        "BLOCK  ",
        "RUNNING",
        "READY  ",
        "EXITED "
    };
    printk("----------- PROCESS TABLE START -----------\n");
    printk("| idx | PID | name             | status  |\n");
    for (int i=0; i<pcb_n; i++) {
        char buf[17] = "                ";
        int len = strlen(pcb[i].name);
        strncpy(buf, pcb[i].name, len<16 ? len : 16);
        printk("| %03d | %03d | %s | %s |\n", i, pcb[i].pid, buf, status_dict[pcb[i].status]);
    }
    printk("------------ PROCESS TABLE END ------------\n");
}

pid_t do_getpid(void) {
    return current_running->pid;
}
