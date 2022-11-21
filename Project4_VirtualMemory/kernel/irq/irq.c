#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/smp.h>
#include <os/loader.h>
#include <printk.h>
#include <assert.h>
#include <csr.h>
#include <screen.h>
#include <pgtable.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // interrupt handler.
    // call corresponding handler by the value of `scause`
    handler_t handler;
    long is_irq = scause & SCAUSE_IRQ_FLAG;
    long code = scause & ~SCAUSE_IRQ_FLAG;
    if (is_irq) {
        handler = irq_table[code];
    } else {
        handler = exc_table[code];
    }
    handler(regs, stval, scause);
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();
}

void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause) {
    int cid = get_current_cpu_id();
    int code = scause & ~SCAUSE_IRQ_FLAG;
    logging(LOG_DEBUG, "pgfault", "%d.%s.%d epc=0x%x, badaddr=0x%x, tp=%s\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid, regs->sepc, stval,
            code == EXCC_INST_PAGE_FAULT ? "INST" : code == EXCC_LOAD_PAGE_FAULT ? "LOAD" : "STORE");

    // get pte
    PTE *pte = get_pte_of(stval, current_running[cid]->pgdir, 0);
    // pte not present
    if (pte == NULL) {
        // check if it's on disk
        if (check_and_swap(current_running[cid], stval) == NULL) {
            // not on disk, try to alloc a new page
            if (alloc_page_helper(stval, current_running[cid]) == 0) {
                // failed to alloc, kill current_running
                printk("kernel panic: alloc page failed\n");
                do_exit();
            }
        }
        pte = get_pte_of(stval, current_running[cid]->pgdir, 0);
    }
    // set attribute
    if (code == EXCC_LOAD_PAGE_FAULT) {
        set_attribute(pte, get_attribute(*pte, _PAGE_CTRL_MASK) | _PAGE_ACCESSED);
    } else if (code == EXCC_STORE_PAGE_FAULT) {
        set_attribute(pte, get_attribute(*pte, _PAGE_CTRL_MASK) | _PAGE_ACCESSED | _PAGE_DIRTY);
    }
    // reflush hardware
    local_flush_tlb_all();
    if (code == EXCC_INST_PAGE_FAULT) {
        local_flush_icache_all();
    }
}

void init_exception()
{
    /* initialize exc_table
     * handle_syscall, handle_other, etc.*/
    for (int i=0; i<EXCC_COUNT; i++)
        exc_table[i] = handle_other;
    exc_table[EXCC_SYSCALL] = handle_syscall;
    exc_table[EXCC_INST_PAGE_FAULT] = handle_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT] = handle_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT] = handle_page_fault;

    /* initialize irq_table
     * handle_int, handle_other, etc.*/
    for (int i=0; i<IRQC_COUNT; i++)
        irq_table[i] = handle_other;
    irq_table[IRQC_S_TIMER] = handle_irq_timer;

    /* set up the entrypoint of exceptions */
    setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
