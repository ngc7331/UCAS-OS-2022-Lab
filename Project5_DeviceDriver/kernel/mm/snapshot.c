#include <os/mm.h>
#include <os/smp.h>
#include <printk.h>

#define SNAPSHOT_LIM (0x1000 * PAGE_SIZE)

uint64_t do_snapshot(uint64_t va) {
    int cid = get_current_cpu_id();
    // align
    va = ROUNDDOWN(va, PAGE_SIZE);
    // set read-only
    PTE *pte = get_pte_of(va, current_running[cid]->pgdir, 0);
    set_attribute(pte, get_attribute(*pte, _PAGE_CTRL_MASK) & ~_PAGE_WRITE);
    // allocate a new va for snapshot
    uint64_t new_va = 0;
    for (uintptr_t i=va; i<va+SNAPSHOT_LIM; i+=PAGE_SIZE) {
        PTE *tmp = get_pte_of(i, current_running[cid]->pgdir, 4);
        if (tmp == NULL || *tmp == 0) {
            // not allocated(pte!=NULL) and not on disk(*pte!=0)
            new_va = i;
            break;
        }
    }
    // map pagedir for new va
    PTE *new_pte = map_page(new_va, current_running[cid]->pgdir, &current_running[cid]->page_list);
    // set attr for new pte
    set_pfn(new_pte, get_pfn(*pte));
    set_attribute(new_pte, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);
    logging(LOG_INFO, "snapshot", "%d.%s.%d create snapshot for 0x%x%x at 0x%x%x\n",
            current_running[cid]->pid, current_running[cid]->name, current_running[cid]->tid,
            va>>32, va, new_va>>32, new_va);
    return new_va;
}

uint64_t do_getpa(uint64_t va) {
    int cid = get_current_cpu_id();
    return kva2pa(get_kva_of(va, current_running[cid]->pgdir));
}
