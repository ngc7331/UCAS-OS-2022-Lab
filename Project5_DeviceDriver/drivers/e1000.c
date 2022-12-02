#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>
#include <printk.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* Initialize tx descriptors */
    for (int i=0; i<TXDESCS; i++) {
        tx_desc_array[i].addr = 0;
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cso = 0;
        tx_desc_array[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
        tx_desc_array[i].status = 0;
        tx_desc_array[i].css = 0;
        tx_desc_array[i].special = 0;
    }

    /* Set up the Tx descriptor base address and length */
    e1000_write_reg(e1000, E1000_TDBAL, kva2pa((uint64_t) tx_desc_array) & 0xffffffff);
    e1000_write_reg(e1000, E1000_TDBAH, kva2pa((uint64_t) tx_desc_array) >> 32);
    e1000_write_reg(e1000, E1000_TDLEN, sizeof(tx_desc_array));

	/* Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Program the Transmit Control Register */
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
                                       (E1000_TCTL_CT & (0x10 << 4)) | (E1000_TCTL_COLD & (0x40 << 12)));
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* Set e1000 MAC Address to RAR[0] */
    e1000_write_reg_array(e1000, E1000_RA, 0, (uint32_t) enetaddr[3] << 24 | (uint32_t) enetaddr[2] << 16 |
                                              (uint32_t) enetaddr[1] << 8 | (uint32_t) enetaddr[0]); // RAL
    e1000_write_reg_array(e1000, E1000_RA, 1, E1000_RAH_AV |
                                              (uint32_t) enetaddr[5] << 8 | (uint32_t) enetaddr[4]); // RAH

    /* Initialize rx descriptors */
    for (int i=0; i<RXDESCS; i++) {
        rx_desc_array[i].addr = kva2pa((uint64_t) &rx_pkt_buffer[i]);
        rx_desc_array[i].length = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
    }

    /* Set up the Rx descriptor base address and length */
    e1000_write_reg(e1000, E1000_RDBAL, kva2pa((uint64_t) rx_desc_array) & 0xffffffff);
    e1000_write_reg(e1000, E1000_RDBAH, kva2pa((uint64_t) rx_desc_array) >> 32);
    e1000_write_reg(e1000, E1000_RDLEN, sizeof(rx_desc_array));

    /* Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);

    /* Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* Transmit one packet from txpacket */
    uint32_t tdh, tdt;
    do {
        tdh = e1000_read_reg(e1000, E1000_TDH);
        tdt = e1000_read_reg(e1000, E1000_TDT);
    } while ((tdt+1) % TXDESCS == tdh);  // full
    logging(LOG_DEBUG, "e1000", "... tdt=%u, tdh=%u\n", tdt, tdh);

    // copy to buf
    memcpy((uint8_t *) &tx_pkt_buffer[tdt], txpacket, length);

    // write desp
    tx_desc_array[tdt].addr = kva2pa((uint64_t) tx_pkt_buffer[tdt]);
    tx_desc_array[tdt].length = length;

    // flush hardware
    local_flush_dcache();

    // set tdt
    e1000_write_reg(e1000, E1000_TDT, (tdt+1) % TXDESCS);

    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer) {
    /* Receive one packet and put it into rxbuffer */
    uint32_t rdh, rdt;
    do {
        rdh = e1000_read_reg(e1000, E1000_RDH);
        rdt = e1000_read_reg(e1000, E1000_RDT);
    } while ((rdt+1) % TXDESCS == rdh);  // empty
    logging(LOG_DEBUG, "e1000", "... rdt=%u, rdh=%u\n", rdt, rdh);
    uint32_t next = (rdt+1) % TXDESCS;

    do {
        // flush hardware
        local_flush_dcache();
    } while (!(rx_desc_array[next].status & E1000_RXD_STAT_DD));  // not ready

    // read desp
    int length = rx_desc_array[next].length;

    // copy from buf
    memcpy(rxbuffer, (uint8_t *) &rx_pkt_buffer[next], length);

    // set rdt
    e1000_write_reg(e1000, E1000_RDT, next);

    return length;
}
