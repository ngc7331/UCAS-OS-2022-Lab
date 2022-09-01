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

static void print_help(void) {
    bios_putstr("[kernel] App list:\n\r");
    for (int i=0; i<tasknum; i++) {
        console_print("\t________________________________\n\r", tasks[i].name);
    }
}

static void init_task_info(void) {
    // Init 'tasks' array via reading app-info sector
    // FIXME: See tools/createimage.c L#225
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

    buf[0] = check ? 't' : 'f';
    buf[1] = version + '0';
    buf[2] = '\0';
    console_print("[kernel] I: bss check=_, version=_\n\r", buf);

    itoa(tasknum, 10, buf, BUFSIZE, 0);
    console_print("[kernel] D: tasknum=__\n\r", buf);

    // Load tasks by task name and then execute them.
    while (1) {
        bios_putstr("> ");

        console_getline(buf, BUFSIZE);
        if (strcmp(buf, "help") == 0) {
            print_help();
            continue;
        }

        int taskid = get_taskid_by_name(buf);
        if (taskid < 0) {
            console_print("[kernel] E: no such app: ________________________________\n\r", buf);
            continue;
        }

        // TEST
        itoa(taskid, 10, buf, BUFSIZE, 0);
        console_print("[kernel] D: taskid=__,", buf);
        itoa(tasks[taskid].entrypoint, 16, buf, BUFSIZE, 8);
        console_print(" entrypoint=__________", buf);
        itoa(tasks[taskid].phyaddr, 16, buf, BUFSIZE, 8);
        console_print(" phyaddr=__________\n\r", buf);

        console_print("[kernel] I: Loading user app: ________________________________...", tasks[taskid].name);

        int (*task)() = (int (*)()) load_task_img(taskid);
        if (task == 0)
            bios_putstr(" error, abort\n\r");
        else {
            bios_putstr(" loaded, running\n\r");
            task();
            bios_putstr("[kernel] I: Finished\n\r");
        }
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1) {
        asm volatile("wfi");
    }

    return 0;
}
