#include <assert.h>
#include <os/fs.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/pthread.h>
#include <os/task.h>
#include <printk.h>

#define SECTOR_SIZE 512
LIST_HEAD(freeswap_list);

unsigned int allocDBlock(int numBlock)
{
    static unsigned int diskptr = 0;  // block id
    if (diskptr == 0)
        diskptr = (*((long *) TASK_INFO_P_LOC) + (appnum + batchnum) * sizeof(task_info_t)) / SECTOR_SIZE + 1;
    else if (diskptr >= FS_START) {
        logging(LOG_CRITICAL, "swap", "no swap space avaliable\n");
        assert(0);
    }
    // align PAGE_SIZE
    unsigned int ret = diskptr;
    diskptr = ret + numBlock;
    return ret;
}

swap_t *alloc_swap1(void) {
    swap_t *swap;
    if (!list_is_empty(&freeswap_list)) {
        swap = list_entry(freeswap_list.next, swap_t, list);
        list_delete(freeswap_list.next);
        logging(LOG_VERBOSE, "swap", "reuse sector at 0x%x\n", swap->pa);
    } else {
        swap = (swap_t *) kmalloc(sizeof(swap_t));
        list_init(&swap->list);
        swap->pa = allocDBlock(PAGE_SIZE / SECTOR_SIZE);
        logging(LOG_VERBOSE, "swap", "allocate new sector at 0x%x\n", swap->pa);
    }
    return swap;
}

void free_swap1(swap_t *swap) {
    list_insert(&freeswap_list, &swap->list);
    logging(LOG_VERBOSE, "swap", "freed sector at 0x%x\n", swap->pa);
}

// FIFO swap
uintptr_t swap_out() {
    logging(LOG_INFO, "swap", "store page to disk\n");
    page_t *page = list_entry(onmem_list.next, page_t, onmem);
    // alloc swap sector
    page->swap = alloc_swap1();
    // delete from onmem
    list_delete(onmem_list.next);
    logging(LOG_DEBUG, "swap", "... from 0x%lx\n", page->kva);
    // set pfn & attr
    PTE *pte = get_pte_of(page->va, page->owner->pgdir, 0);
    set_attribute(pte, get_attribute(*pte, _PAGE_CTRL_MASK) & ~_PAGE_PRESENT);
    // store to disk
    bios_sdwrite(kva2pa(page->kva), PAGE_SIZE/SECTOR_SIZE, page->swap->pa);
    logging(LOG_DEBUG, "swap", "... pid=%d, va=0x%lx, diskptr=0x%x\n", page->owner->pid, page->va, page->swap->pa);
    // reset kva
    uintptr_t kva = page->kva;
    page->kva = 0;
    return kva;
}

void swap_in(page_t *page, uintptr_t kva) {
    logging(LOG_INFO, "swap", "load page from disk\n");
    logging(LOG_DEBUG, "swap", "... to 0x%lx\n", kva);
    // reset kva
    page->kva = kva;
    // load from disk
    bios_sdread(kva2pa(kva), PAGE_SIZE/SECTOR_SIZE, page->swap->pa);
    logging(LOG_DEBUG, "swap", "... pid=%d, va=0x%lx, diskptr=0x%x\n", page->owner->pid, page->va, page->swap->pa);
    // free swap sector
    free_swap1(page->swap);
    page->swap = NULL;
    // set pfn & attr
    PTE *pte = get_pte_of(page->va, page->owner->pgdir, 4);
    set_pfn(pte, kva2pa(kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(pte, get_attribute(*pte, _PAGE_CTRL_MASK) | _PAGE_PRESENT);
    // insert into onmem
    list_insert(onmem_list.prev, &page->onmem);
}

page_t *check_and_swap(pcb_t *pcb, uintptr_t va) {
    if (pcb->type == TYPE_THREAD)
        pcb = get_parent(pcb->pid);
    for (list_node_t *p = pcb->page_list.next; p!=&pcb->page_list; p=p->next) {
        page_t *page = list_entry(p, page_t, list);
        if (page->va != (va & ~(PAGE_SIZE-1))) {
            continue;
        }
        if (page->swap == NULL) {
            if (page->tp != PAGE_USER)
                continue;
            logging(LOG_ERROR, "swap", "page record found, but not on disk, kva=0x%lx\n", page->kva);
            return NULL;
        }
        uintptr_t kva = swap_out();
        swap_in(page, kva);
        return page;
    }
    return NULL;
}
