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

    while (e1000_transmit(txpacket, length) == -1) {
        // Call do_block when e1000 transmit queue is full
        logging(LOG_DEBUG, "net", "send queue full, block\n");
        do_block(current_running[cid], &send_block_queue);
    }

    check_net_send();

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
        while ((pkt_lens[i] = e1000_poll(rxbuffer + offset)) == -1) {
            // Call do_block when there is no packet on the way
            logging(LOG_DEBUG, "net", "recv queue empty, block\n");
            do_block(current_running[cid], &recv_block_queue);
        }
        logging(LOG_DEBUG, "net", "... pkt[%d] offset = %d, length = %d\n", i, offset, pkt_lens[i]);
        offset += pkt_lens[i];
    }

    check_net_recv();

    return 0;  // Bytes it has received
}

void net_handle_irq(void) {
    // TODO: [p5-task4] Handle interrupts from network device
}

void check_net_send() {
    if (!check_tx() || list_is_empty(&send_block_queue))
        return ;
    do_unblock(&send_block_queue);
}

void check_net_recv() {
    if (!check_rx() || list_is_empty(&recv_block_queue))
        return ;
    do_unblock(&recv_block_queue);
}
