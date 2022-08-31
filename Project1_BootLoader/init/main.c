#include <asm.h>
#include <common.h>
#include <os/bios.h>
#include <os/console.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/task.h>
#include <os/utils.h>
#include <type.h>

#define BUFSIZE 50
#define TASK_NUM_LOC 0x502001fa

int version = 2; // version must between 0 and 9
char buf[BUFSIZE];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void) {
    for (int i = 0; i < BUFSIZE; ++i)
    {
        if (buf[i] != 0)
        {
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
}

int main(void) {
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by BIOS (ΦωΦ)
    init_bios();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "[kernel] bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i) {
        buf[i] = output_str[i];
        if (buf[i] == '_') {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("[kernel] Hello OS!\n\r");
    bios_putstr(buf);

    short tasknum = *((int *) TASK_NUM_LOC);
    bios_putstr("[kernel] tasknum: ");
    bios_putchar(tasknum / 10 % 10 + '0');
    bios_putchar(tasknum % 10 + '0');
    bios_putstr("\n\r");

    // TODO: Load tasks by task name [p1-task4] and then execute them.
    while (1) {
        bios_putstr("[kernel] Input task id: ");
        int len = console_getline(buf, BUFSIZE);
        int taskid = atoi(buf, len);
        if (taskid >= 0 && taskid < tasknum) {
            bios_putstr("[kernel] Loading user app#");
            bios_putchar(taskid + '0');
            bios_putstr("\n\r");
            int (*task)() = load_task_img(taskid);
            if (task == 0)
                bios_putstr("[kernel] Error: Load error, abort\n\r");
            else {
                bios_putstr("[kernel] Loaded, running\n\r");
                task();
                bios_putstr("[kernel] Finished\n\r");
            }
        }
        else {
            bios_putstr("[kernel] Error: task id out of range: (0, ");
            bios_putchar((tasknum-1) / 10 % 10 + '0');
            bios_putchar((tasknum-1) % 10 + '0');
            bios_putstr(")\n\r");
        }
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1) {
        asm volatile("wfi");
    }

    return 0;
}
