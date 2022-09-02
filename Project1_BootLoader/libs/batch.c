#include <os/batch.h>

int batch_execute(int batchid) {
    batch_info_t batch = batchs[batchid];

    console_print("[kernel] I: ===== executing batch: ________________"
                  "________________ =====\n\r", batch.name);

    char *file = (char *) load_img(BATCH_MEM_BASE, batch.phyaddr,
                                   batch.size, FALSE);
    char buf[64];
    int i;
    while (*file) {
        while (*file && *file != '\n')
            buf[i++] = *file++;
        file += *file == '\n';
        buf[i] = '\0';
        console_print(" -> executing ________________________________\n\r", buf);
        int taskid = get_taskid_by_name(buf);
        ((int (*)()) load_task_img(taskid))();
        i=0;
    }

    console_print("[kernel] I: ===== completed batch: ________________"
                  "________________ =====\n\r", batch.name);

    return 0;
}

int get_batchid_by_name(char *name) {
    for (int i=0; i<batchnum; i++) {
        if (strcmp(name, batchs[i].name) == 0)
            return i;
    }
    return -1;
}
