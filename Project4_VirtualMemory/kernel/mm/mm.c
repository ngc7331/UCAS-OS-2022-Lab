#include <os/mm.h>
#include <printk.h>
#include <pgtable.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

LIST_HEAD(freepage_list);

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;
}
#endif

page_t *alloc_page1(void) {
    page_t *page;
    if (!list_is_empty(&freepage_list)) {
        page = list_entry(freepage_list.next, page_t, list);
        list_delete(freepage_list.next);
        page->ref ++;
        logging(LOG_DEBUG, "mm", "reuse page at 0x%x%x\n", page->kva << 32, page->kva);
    } else {
        page = (page_t *) kmalloc(sizeof(page_t));
        page->kva = allocPage(1);
        list_init(&page->list);
        page->ref = 1;
        logging(LOG_DEBUG, "mm", "allocated a new page at 0x%x%x\n", page->kva << 32, page->kva);
    }
    return page;
}

void free_page1(page_t *page) {
    if (--page->ref <= 0) {
        list_delete(&page->list);
        list_insert(&freepage_list, &page->list);
        logging(LOG_DEBUG, "mm", "freed page at 0x%x%x\n", page->kva << 32, page->kva);
    }
}

void *kmalloc(size_t size) {
    if (size > PAGE_SIZE) {
        logging(LOG_ERROR, "mm", "currently unable to kmalloc mem larger than 4K\n");
        return NULL;
    }
    static size_t remaining = 0;
    static ptr_t p = 0;
    if (remaining < size) {
        // NOTE: this can't be freed
        remaining = PAGE_SIZE;
        p = allocPage(1);
        logging(LOG_INFO, "mm", "allocated a new page at 0x%x%x for kmalloc\n", p<<32, p);
    }
    remaining -= size;
    void *ret = (void *) p;
    p += size;
    return ret;
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir) {
    // FIXME: share_pgtable
    PTE *src = (PTE *) src_pgdir;
    PTE *dest = (PTE *) dest_pgdir;
    for (int i=0; i<NUM_PTE_ENTRY; i++) {
        dest[i] = src[i];
    }
}

/* allocate physical page for `va`, mapping it into `pcb->pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb) {
    // 3 level pgtables
    PTE *pt2 = (PTE *) pcb->pgdir;
    PTE *pt1 = NULL;
    PTE *pt0 = NULL;

    // split va into vpns
    uint64_t vpn2 = getvpn2(va);
    uint64_t vpn1 = getvpn1(va);
    uint64_t vpn0 = getvpn0(va);

    logging(LOG_INFO, "mm", "allocate page for addr %x%x in pgtable at %x%x\n", va>>32, va, pcb->pgdir>>32, pcb->pgdir);
    logging(LOG_VERBOSE, "mm", "... vpn2=%x, vpn1=%x, vpn0=%x\n", vpn2, vpn1, vpn0);

    // find level-1 pgtable
    if (!(pt2[vpn2] & _PAGE_PRESENT)) {
        // alloc a new second-level page directory
        page_t *tmp = alloc_page1();
        list_insert(&pcb->page_list, &tmp->list);
        uintptr_t page = tmp->kva;
        set_pfn(&pt2[vpn2], kva2pa(page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pt2[vpn2], _PAGE_PRESENT);
        clear_pgdir(page);
        pt1 = (PTE *) page;
    } else {
        pt1 = (PTE *) pa2kva(get_pa(pt2[vpn2]));
    }

    logging(LOG_VERBOSE, "mm", "... level-1 pgtable at %x%x\n", (uint64_t) pt1 >> 32, (uint64_t)pt1);

#ifdef S_CORE
    // find pte
    PTE *pte = &pt1[vpn1];
#else
    // find level-0 pgtable
    if (!(pt1[vpn1] & _PAGE_PRESENT)) {
        // alloc a new second-level page directory
        page_t *tmp = alloc_page1();
        list_insert(&pcb->page_list, &tmp->list);
        uintptr_t page = tmp->kva;
        set_pfn(&pt1[vpn1], kva2pa(page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pt1[vpn1], _PAGE_PRESENT);
        clear_pgdir(page);
        pt0 = (PTE *) page;
    } else {
        pt0 = (PTE *) pa2kva(get_pa(pt1[vpn1]));
    }

    logging(LOG_VERBOSE, "mm", "... level-0 pgtable at %x%x\n", (uint64_t) pt0 >> 32, (uint64_t)pt0);

    // find pte
    PTE *pte = &pt0[vpn0];
#endif

    logging(LOG_VERBOSE, "mm", "... pte at %x%x\n", (uint64_t) pte >> 32, (uint64_t)pte);

    // FIXME: conflict?
    if (*pte & _PAGE_PRESENT) {
        logging(LOG_ERROR, "mm", "va %lx already in pgdir %lx\n", va, pcb->pgdir);
        return 0;
    }

    // allocate a new page for va
#ifdef S_CORE
    uintptr_t page = allocLargePage(1);
#else
    page_t *tmp = alloc_page1();
    list_insert(&pcb->page_list, &tmp->list);
    uintptr_t page = tmp->kva;
#endif

    logging(LOG_DEBUG, "mm", "... allocated page at %x%x\n", (uint64_t) page >> 32, (uint64_t)page);

    // set pgtable
    set_pfn(pte, kva2pa(page) >> NORMAL_PAGE_SHIFT);
    set_attribute(pte, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                             _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER);

    return page;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    return 0;
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}
