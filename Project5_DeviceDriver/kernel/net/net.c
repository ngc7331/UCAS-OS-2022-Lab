#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <printk.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length) {
    // Transmit one network packet via e1000 device
    int cid = get_current_cpu_id();
    logging(LOG_INFO, "net", "%d.%s.%d send from 0x%lx, length = %d\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, (uint64_t) txpacket, length);

    e1000_transmit(txpacket, length);

    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full

    return length;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens) {
    // Receive one network packet via e1000 device
    int cid = get_current_cpu_id();
    logging(LOG_INFO, "net", "%d.%s.%d recv to 0x%lx, num = %d\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, (uint64_t) rxbuffer, pkt_num);

    int offset = 0;
    for (int i=0; i<pkt_num; i++) {
        pkt_lens[i] = e1000_poll(rxbuffer + offset);
        logging(LOG_DEBUG, "net", "... pkt[%d] offset = %d, length = %d\n", i, offset, pkt_lens[i]);
        offset += pkt_lens[i];
    }

    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return 0;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
}