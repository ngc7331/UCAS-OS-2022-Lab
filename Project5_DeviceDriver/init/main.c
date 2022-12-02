#include <asm.h>
#include <assert.h>
#include <asm/unistd.h>
#include <common.h>
#include <csr.h>
#include <os/ioremap.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <os/pthread.h>
#include <e1000.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

// tasks
task_info_t apps[APP_MAXNUM];
short appnum;
task_info_t batchs[BATCH_MAXNUM];
short batchnum;

static void unmap_boot_addr(void) {
    PTE *pgdir = (PTE *)pa2kva(PGDIR_PA);
    for (uint64_t pa = 0x50000000lu; pa < 0x50400000lu;
         pa += 0x200000lu) {
        uint64_t vpn2 = (pa & VA_MASK) >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        pgdir[vpn2] = 0;
    }
    local_flush_tlb_all();
}

static void init_jmptab(void) {
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
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
    task_info_t *task = (task_info_t *) load_img_tmp(phyaddr, sizeof(task_info_t) * tasknum);
    for (int i=0; i<tasknum; i++) {
        if (task->type == APP) {
            apps[appnum].loaded = 0;
            apps[appnum++] = *task++;
        } else {
            batchs[batchnum].loaded = 0;
            batchs[batchnum++] = *task++;
        }
    }
}

static void init_pid0_pcb(void) {
    int cid = get_current_cpu_id();
    // set sp
    pid0_pcb[cid].kernel_sp = pid0_pcb[cid].kernel_stack_base = pid0_stack[cid] + 2 * PAGE_SIZE;
    pid0_pcb[cid].kernel_sp -= sizeof(regs_context_t);
    // user_sp will be saved by switch_to()

    // init list
    list_init(&pid0_pcb[cid].list);
    list_init(&pid0_pcb[cid].wait_list);

    // set pagedir
    pid0_pcb[cid].pgdir = pa2kva(PGDIR_PA);

    // set identifier
    pid0_pcb[cid].pid = pid0_pcb[cid].tid = 0;
    pid0_pcb[cid].type = TYPE_PROCESS;
    strcpy(pid0_pcb[cid].name, "init");

    // set cpu
    pid0_pcb[cid].cid = cid;
    pid0_pcb[cid].mask = 0xFFFF;

    // set cursor
    pid0_pcb[cid].cursor_x = pid0_pcb[cid].cursor_y = 0;

    // set status
    pid0_pcb[cid].status = TASK_RUNNING;

    // initialize 'current_running'
    current_running[cid] = &pid0_pcb[cid];
    asm volatile(
        "add tp, %0, zero\n\r"
        :
        : "r"(current_running[cid])
    );
}

static void invalid_syscall(long arg0, long arg1, long arg2, long arg3, long arg4) {
    logging(LOG_CRITICAL, "init", "invalid syscall\n");
    logging(LOG_CRITICAL, "init", "... args[]=%ld %ld %ld %ld %ld\n", arg0, arg1, arg2, arg3, arg4);
    printk("invalid syscall\n");
    assert(0);
}

static void init_syscall(void) {
    // initialize system call table.
    // see arch/riscv/include/asm/unistd.h
    for (int i=0; i<NUM_SYSCALLS; i++)
        syscall[i] = (long (*)()) invalid_syscall;
    syscall[SYSCALL_EXEC]          = (long (*)()) do_exec;
    syscall[SYSCALL_EXIT]          = (long (*)()) do_exit;
    syscall[SYSCALL_SLEEP]         = (long (*)()) do_sleep;
    syscall[SYSCALL_KILL]          = (long (*)()) do_kill;
    syscall[SYSCALL_WAITPID]       = (long (*)()) do_waitpid;
    syscall[SYSCALL_PS]            = (long (*)()) do_process_show;
    syscall[SYSCALL_GETPID]        = (long (*)()) do_getpid;
    syscall[SYSCALL_YIELD]         = (long (*)()) do_scheduler;
    syscall[SYSCALL_TASKSET]       = (long (*)()) do_taskset;
    syscall[SYSCALL_WRITE]         = (long (*)()) screen_write;
    syscall[SYSCALL_READCH]        = (long (*)()) bios_getchar;
    syscall[SYSCALL_CURSOR]        = (long (*)()) screen_move_cursor;
    syscall[SYSCALL_REFLUSH]       = (long (*)()) screen_reflush;
    syscall[SYSCALL_CLEAR]         = (long (*)()) screen_clear;
    syscall[SYSCALL_CURSOR_R]      = (long (*)()) screen_move_cursor_r;
    syscall[SYSCALL_SET_SC_BASE]   = (long (*)()) screen_set_scroll_base;
    syscall[SYSCALL_GET_TIMEBASE]  = (long (*)()) get_time_base;
    syscall[SYSCALL_GET_TICK]      = (long (*)()) get_ticks;
    syscall[SYSCALL_LOCK_INIT]     = (long (*)()) do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]      = (long (*)()) do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]  = (long (*)()) do_mutex_lock_release;
    syscall[SYSCALL_SHOW_TASK]     = (long (*)()) do_task_show;
    syscall[SYSCALL_BARR_INIT]     = (long (*)()) do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]     = (long (*)()) do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]  = (long (*)()) do_barrier_destroy;
    syscall[SYSCALL_COND_INIT]     = (long (*)()) do_condition_init;
    syscall[SYSCALL_COND_WAIT]     = (long (*)()) do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]   = (long (*)()) do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST]= (long (*)()) do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]  = (long (*)()) do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN]     = (long (*)()) do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]    = (long (*)()) do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]     = (long (*)()) do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]     = (long (*)()) do_mbox_recv;
    syscall[SYSCALL_SHM_GET]       = (long (*)()) shm_page_get;
    syscall[SYSCALL_SHM_DT]        = (long (*)()) shm_page_dt;
    syscall[SYSCALL_SNAPSHOT]      = (long (*)()) do_snapshot;
    syscall[SYSCALL_GETPA]         = (long (*)()) do_getpa;
    syscall[SYSCALL_PTHREAD_CREATE]= (long (*)()) pthread_create;
    syscall[SYSCALL_PTHREAD_JOIN]  = (long (*)()) pthread_join;
    syscall[SYSCALL_PTHREAD_EXIT]  = (long (*)()) pthread_exit;
    syscall[SYSCALL_NET_SEND]      = (long (*)()) do_net_send;
    syscall[SYSCALL_NET_RECV]      = (long (*)()) do_net_recv;
}

void init_shell(void) {
#ifdef S_CORE_P3
    do_exec(0, 0, 0, 0, 0);
#else
    do_exec("shell", 0, NULL);
#endif
}

int main(void) {
    if (get_current_cpu_id() == 0) {
        // for master core
        // set log level
        set_loglevel(LOG_VERBOSE);

        // Init jump table provided by BIOS (ΦωΦ)
        init_jmptab();
        logging(LOG_INFO, "init", "Jump table initialization succeeded.\n");

        // Init task information (〃'▽'〃)
        init_task_info();
        logging(LOG_INFO, "init", "Task info loaded, apps=%d, batches=%d.\n", appnum, batchnum);

        // preload apps
        // FIXME: this is to prevent a QEMU bug on calling bios_sdread()
        // preload_apps();
        // logging(LOG_INFO, "init", "APPs preloaded.\n");

        // Init Process Control Blocks |•'-'•) ✧
        init_pcbs();
        logging(LOG_INFO, "init", "PCB[] initialization succeeded.\n");
        init_pid0_pcb();
        logging(LOG_INFO, "init", "PID0_PCB initialization succeeded.\n");

        // Read Flatten Device Tree (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        e1000 = (volatile uint8_t *)bios_read_fdt(EHTERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        logging(LOG_INFO, "init", "e1000: 0x%lx, plic_addr: 0x%lx, nr_irqs: 0x%lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        logging(LOG_INFO, "init", "IOremap initialization succeeded.\n");

        // Init lock mechanism o(´^｀)o
        init_locks();
        init_barriers();
        init_conditions();
        init_mbox();
        logging(LOG_INFO, "init", "Lock mechanism initialization succeeded.\n");

        // TODO: [p5-task4] Init plic
        // plic_init(plic_addr, nr_irqs);
        // logging(LOG_INFO, "init", "PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // Init network device
        e1000_init();
        logging(LOG_INFO, "init", "E1000 device initialized successfully.\n");

        // Init interrupt (^_^)
        init_exception();
        logging(LOG_INFO, "init", "Interrupt processing initialization succeeded.\n");

        // Init smp
        smp_init();
        logging(LOG_INFO, "init", "SMP initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        logging(LOG_INFO, "init", "System call initialization succeeded.\n");

        // Init screen (QAQ)
        init_screen();
        logging(LOG_INFO, "init", "SCREEN initialization succeeded.\n");

        // Init shell
        init_shell();
        logging(LOG_INFO, "init", "Shell initialization succeeded.\n");

        // wake up slave
        logging(LOG_INFO, "init", "Wake up slave core.\n");
        wakeup_other_hart();
    } else {
        // for slave core
        // FIXME: when to? upmap boot address
        unmap_boot_addr();
        logging(LOG_INFO, "init", "Unmapped boot address[0x50000000, 0x54000000)\n");

        // init pcb
        init_pid0_pcb();
        logging(LOG_INFO, "init", "PID0_PCB initialization succeeded.\n");

        // setup exception
        setup_exception();
        logging(LOG_INFO, "init", "Interrupt processing initialization succeeded.\n");
    }

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
