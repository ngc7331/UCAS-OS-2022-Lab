#include <os/string.h>
#include <os/task.h>
#include <printk.h>

int get_taskid_by_name(char *name, task_type_t type) {
    task_info_t *tasks = type == APP ? apps : batchs;
    int num = type == APP ? appnum : batchnum;
    for (int i=0; i<num; i++) {
        if (strcmp(name, tasks[i].name) == 0)
            return i;
    }
    return -1;
}

void do_task_show(void) {
    const int cols = 3;
    printk("------------------------------ TASK TABLE START ------------------------------\n");
    printk("| idx | name             || idx | name             || idx | name             |\n");
    for (int i=0; i<appnum; i+=cols) {
        for (int j=0; j<cols; j++) {
            if (i + j >= appnum) {
                printk("| ... | ...              |");  // print a placeholder
            } else {
                char buf[17] = "                ";
                int len = strlen(apps[i+j].name);
                strncpy(buf, apps[i+j].name, len<16 ? len : 16);
                if (len > 16)
                    buf[13] = buf[14] = buf[15] = '.';
                printk("| %03d | %s |", i+j, buf);
            }
        }
        printk("\n");
    }
    printk("------------------------------- TASK TABLE END -------------------------------\n");
}
