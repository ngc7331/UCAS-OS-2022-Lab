#include <asm.h>
#include <common.h>
#include <os/bios.h>
#include <os/console.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/task.h>
#include <os/utils.h>
#include <type.h>

#define BUFSIZE 64
#define TASK_NUM_LOC 0x502001fa

int version = 2; // version must between 0 and 9
char buf[BUFSIZE];

short tasknum;
// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void) {
    for (int i = 0; i < BUFSIZE; ++i) {
        if (buf[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static void init_bios(void) {
    volatile long (*(*jmptab))() = (volatile long (*(*))())BIOS_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void) {
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    tasknum = *((int *) TASK_NUM_LOC);
    task_info_t *task = (task_info_t *) (TASK_NUM_LOC - sizeof(task_info_t) * tasknum);
    for (short i=0; i<tasknum; i++, task++)
        tasks[i] = *task;
}

int main(void) {
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by BIOS (ΦωΦ)
    init_bios();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    bios_putstr("[kernel] Hello OS!\n\r");

    char output_bss_check_val[2] = {
        check ? 't' : 'f',
        version + '0'
    };
    console_print("[kernel] I: bss check=_, version=_\n\r", output_bss_check_val);

    char output_tasknum_val[2] = {
        tasknum / 10 % 10 + '0',
        tasknum % 10 + '0'
    };
    console_print("[kernel] I: tasknum: __\n\r", output_tasknum_val);

    // TODO: Load tasks by task name [p1-task4] and then execute them.
    while (1) {
        bios_putstr("[kernel] Input task id: ");

        int len = console_getline(buf, BUFSIZE);
        int taskid = atoi(buf, len);

        if (taskid >= 0 && taskid < tasknum) {
            char output_load_app[1] = {taskid + '0'};
            console_print("[kernel] I: Loading user app#_\n\r", output_load_app);
            bios_putstr("[kernel] I: Taskname: ");
            bios_putstr(tasks[taskid].name);
            bios_putstr("\n\r");

            int (*task)() = (int (*)()) load_task_img(taskid);
            if (task == 0)
                bios_putstr("[kernel] E: Load error, abort\n\r");
            else {
                bios_putstr("[kernel] I: Loaded, running\n\r");
                task();
                bios_putstr("[kernel] I: Finished\n\r");
            }
        }
        else {
            char output_load_err[2] = {
                (tasknum-1) / 10 % 10 + '0',
                (tasknum-1) % 10 + '0'
            };
            console_print("[kernel] E: task id out of range: (0, __)\n\r", output_load_err);
        }
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1) {
        asm volatile("wfi");
    }

    return 0;
}
