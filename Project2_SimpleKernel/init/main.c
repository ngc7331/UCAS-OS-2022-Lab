#include <asm.h>
#include <assert.h>
#include <asm/unistd.h>
#include <common.h>
#include <csr.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

#define TASK_NUM_LOC 0x502001fa
#define TASK_INFO_P_LOC (TASK_NUM_LOC - 8)

extern void ret_from_exception();

// last allocated pid
int pid_n = 0;

// tasks
task_info_t apps[APP_MAXNUM];
short appnum;
task_info_t batchs[BATCH_MAXNUM];
short batchnum;

static void init_jmptab(void) {
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

}

static void init_task_info(void) {
    // Init 'tasks' array via reading app-info sector
    // load pointer from TASK_INFO_P_LOC
    appnum = batchnum = 0;
    long phyaddr = *((long *) TASK_INFO_P_LOC);
    short tasknum = *((short *) TASK_NUM_LOC);
    // read img to some random memory
    task_info_t *task = (task_info_t *) load_img(APP_MEM_BASE, phyaddr,
                                                 sizeof(task_info_t) * tasknum, FALSE);
    for (int i=0; i<tasknum; i++) {
        if (task->type == APP)
            apps[appnum++] = *task++;
        else
            batchs[batchnum++] = *task++;
    }
}

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb) {
     /* initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    for (int i=0; i<32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;
    pt_regs->sstatus = SR_SPIE;

    pt_regs->sepc = entry_point;
    pt_regs->regs[1] = entry_point;
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

void pcb_enqueue(list_node_t *queue, pcb_t *pcb) {
    list_insert(queue->prev, &pcb->list);
}

pcb_t *pcb_dequeue(list_node_t *queue) {
    pcb_t *pcb = list_entry(queue->next, pcb_t, list);
    list_delete(queue->next);
    return pcb;
}

static void init_pcb(void) {
    // load needed tasks and init their corresponding PCB
    for (int i=0; i<appnum; i++) {
        load_task_img(i, APP);
        pcb[i].kernel_sp = allocKernelPage(1) + PAGE_SIZE;
        pcb[i].user_sp = allocUserPage(1) + PAGE_SIZE;
        pcb[i].pid = ++pid_n;
        strcpy(pcb[i].name, apps[i].name);
        pcb[i].cursor_x = pcb[i].cursor_y = 0;
        pcb[i].status = TASK_READY;

        logging(LOG_DEBUG, "init", "load %s as pid=%d\n", pcb[i].name, pcb[i].pid);
        logging(LOG_VERBOSE, "init", "...entrypoint=%x kernel_sp=%x user_sp=%x\n", apps[i].entrypoint, pcb[i].kernel_sp, pcb[i].user_sp);

        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, apps[i].entrypoint, &pcb[i]);

        pcb_enqueue(&ready_queue, &pcb[i]);
    }

    // set pid0_pcb.kernel_sp
    pid0_pcb.kernel_sp -= sizeof(regs_context_t);

    // remember to initialize 'current_running'
    current_running = &pid0_pcb;
    asm volatile(
        "add tp, %0, zero\n\r"
        :
        : "r"(current_running)
    );
}

static void init_syscall(void) {
    // initialize system call table.
    // see arch/riscv/include/asm/unistd.h
    syscall[SYSCALL_SLEEP]        = (long (*)())&do_sleep;
    syscall[SYSCALL_YIELD]        = (long (*)())&do_scheduler;
    syscall[SYSCALL_WRITE]        = (long (*)())&screen_write;
    syscall[SYSCALL_CURSOR]       = (long (*)())&screen_move_cursor;
    syscall[SYSCALL_REFLUSH]      = (long (*)())&screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())&get_time_base;
    syscall[SYSCALL_GET_TICK]     = (long (*)())&get_ticks;
    syscall[SYSCALL_LOCK_INIT]    = (long (*)())&do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]     = (long (*)())&do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())&do_mutex_lock_release;
}

int main(void) {
    // Init jump table provided by BIOS (ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // set log level
    set_loglevel(LOG_DEBUG);

    // Init Process Control Blocks |•'-'•) ✧
    logging(LOG_INFO, "init", "Initialize PCBs...\n");
    init_pcb();
    logging(LOG_INFO, "init", "PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    logging(LOG_INFO, "init", "Lock mechanism initialization succeeded.\n");

     // Init interrupt (^_^)
    init_exception();
    logging(LOG_INFO, "init", "Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    logging(LOG_INFO, "init", "System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    logging(LOG_INFO, "init", "SCREEN initialization succeeded.\n");

    // Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    enable_interrupt();
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    logging(LOG_INFO, "init", "Timer initialization succeeded & interrupt enabled.\n");

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1) {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
