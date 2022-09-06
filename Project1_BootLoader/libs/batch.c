#include <os/batch.h>

int run_batch(int batchid) {
    batch_info_t batch = batchs[batchid];

    int err = 0;

    console_print("[kernel] I: ===== executing batch: ________________"
                  "________________ =====\n\r", batch.name);

    char *file = (char *) load_img(BATCH_MEM_BASE, batch.phyaddr,
                                   batch.size, FALSE);
    char buf[512];
    while (*file) {
        int i=0;
        while (*file && *file != '\n')
            buf[i++] = *file++;
        file += *file == '\n';

        if (i == 0) continue;
        buf[i] = '\0';

        console_print(" -> executing ________________________________\n\r", buf);
        int taskid = get_taskid_by_name(buf);
        if (taskid < 0) {
            err = 1;
            bios_putstr("[kernel] E: Task not found, please check your batch file\n\r");
        } else {
            int (*task)() = (int (*)()) load_task_img(taskid);
            if (task == 0) {
                err = 1;
                bios_putstr("[kernel] E: Task load error\n\r");
            } else {
                task();
            }
        }
    }

    console_print("[kernel] I: ===== completed batch: ________________"
                  "________________ =====\n\r", batch.name);

    return err;
}

int get_batchid_by_name(char *name) {
    for (int i=0; i<batchnum; i++) {
        if (strcmp(name, batchs[i].name) == 0)
            return i;
    }
    return -1;
}
