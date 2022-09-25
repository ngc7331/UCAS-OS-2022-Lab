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
#include <os/utils.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

#define TASK_NUM_LOC 0x502001fa
#define TASK_INFO_P_LOC (TASK_NUM_LOC - 8)

extern void ret_from_exception();

// required tasks
// p2-task1
#define NEEDED_TASKS {"print1", "print2", "fly"}
#define NEEDED_TASK_NUM 3
// p2-task2
// #define NEEDED_TASKS {"print1", "print2", "fly", "lock1", "lock2"}
// #define NEEDED_TASK_NUM 5

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
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    // set sp to simulate just returning from switch_to
    /* NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    pcb->kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
    pcb->user_sp = user_stack;

    // save regs to kernel_stack
    pt_switchto->regs[0] = entry_point;
    pt_switchto->regs[1] = kernel_stack;
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

static void init_pcb(void)
{
    // load needed tasks and init their corresponding PCB
    char *needed_tasks[] = NEEDED_TASKS;
    for (int i=0; i<NEEDED_TASK_NUM; i++) {
        int id = -1;
        for (int j=0; j<appnum; j++) {
            if (strcmp(needed_tasks[i], apps[j].name) == 0) {
                id = j;
                break;
            }
        }
        if (id == -1) {
            printk("> [INIT] Failed to init task: %s", needed_tasks[i]);
            continue;
        }
        load_task_img(id, APP);
        pcb[i].kernel_sp = allocKernelPage(1);
        pcb[i].user_sp = allocUserPage(1);
        pcb[i].pid = ++pid_n;
        pcb[i].status = TASK_READY;
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, apps[id].entrypoint, &pcb[i]);
        pcb_enqueue(&ready_queue, &pcb[i]);
    }

    // remember to initialize 'current_running'
    current_running = &pid0_pcb;
}

static void init_syscall(void) {
    // TODO: [p2-task3] initialize system call table.
}

int main(void) {
    // Init jump table provided by BIOS (ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

     // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1) {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
