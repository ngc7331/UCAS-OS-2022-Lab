#include <os/batch.h>

int batch_execute(int batchid) {
    batch_info_t batch = batchs[batchid];
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

    return 0;
}
