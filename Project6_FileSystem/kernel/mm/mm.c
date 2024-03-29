#include <assert.h>
#include <os/mm.h>
#include <os/pthread.h>
#include <os/string.h>
#include <printk.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

// limit page frame to test swap
/* NOTE: only pages used by user is caculated
 * pgdir / kernel stack are excluded
 */
#define PAGEFRAME_LIMIT 20
unsigned remaining_pf = PAGEFRAME_LIMIT;

LIST_HEAD(freepage_list);
LIST_HEAD(onmem_list);

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
        list_delete(&page->onmem);
        logging(LOG_VERBOSE, "mm", "reuse page at 0x%lx\n", page->kva);
    } else {
        page = (page_t *) kmalloc(sizeof(page_t));
        page->kva = allocPage(1);
        list_init(&page->list);
        list_init(&page->onmem);
        logging(LOG_VERBOSE, "mm", "allocated a new page at 0x%lx\n", page->kva);
    }
    page->tp = PAGE_KERNEL;
    page->va = 0;
    page->swap = NULL;
    page->owner = NULL;
    memset((void *) page->kva, 0, PAGE_SIZE);
    return page;
}

void free_page1(page_t *page) {
    list_delete(&page->list);
    if (page->kva == 0) { // not on memory
        free_swap1(page->swap);
        return ;
    }
    list_delete(&page->onmem);
    list_insert(&freepage_list, &page->list);
    if (page->tp == PAGE_USER)
        remaining_pf ++;
    logging(LOG_VERBOSE, "mm", "freed page at 0x%lx\n", page->kva);
}

void *kmalloc(size_t size) {
    size = ROUND(size, 4);
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
        logging(LOG_INFO, "mm", "allocated a new page at 0x%lx for kmalloc\n", p);
    }
    remaining -= size;
    void *ret = (void *) p;
    p += size;
    return ret;
}

void do_garbage_collector(void) {
    for (int i=0; i<NUM_MAX_TASK; i++) {
        if (pcb[i].status == TASK_UNUSED)
            break;
        if (pcb[i].status != TASK_EXITED || pcb[i].type != TYPE_PROCESS)
            continue;
        while (!list_is_empty(&pcb[i].page_list)) {
            free_page1(list_entry(pcb[i].page_list.next, page_t, list));
        }
    }
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir) {
    PTE *src = (PTE *) src_pgdir;
    PTE *dest = (PTE *) dest_pgdir;
    for (int i=0; i<NUM_PTE_ENTRY; i++) {
        dest[i] = src[i];
    }
}

list_node_t *get_page_list(pcb_t *pcb) {
    list_node_t *page_list;
    if (pcb->type == TYPE_PROCESS)
        page_list = &pcb->page_list;
    else {
        logging(LOG_DEBUG, "mm", "use parent's page_list\n");
        page_list = &get_parent(pcb->pid)->page_list;
    }
    return page_list;
}

/* allocate physical page for `va`, mapping it into `pcb->pgdir`,
   return the kernel virtual address for the page
   */
PTE *map_page(uintptr_t va, uint64_t pgdir, list_node_t *page_list, int level) {
    if (level != 0 && level != 1) {
        logging(LOG_ERROR, "mm", "map_page: invalid arg, level must be 0 or 1\n");
        return NULL;
    }

#ifdef S_CORE
    level = 1;
#endif

    // 3 level pgtables
    PTE *pt2 = (PTE *) pgdir;
    PTE *pt1 = NULL;
    PTE *pt0 = NULL;

    // split va into vpns
    uint64_t vpn2 = getvpn2(va);
    uint64_t vpn1 = getvpn1(va);
    uint64_t vpn0 = getvpn0(va);

    logging(LOG_INFO, "mm", "allocate page for addr 0x%lx in pgtable at 0x%lx\n", va, pgdir);
    logging(LOG_VV, "mm", "... vpn2=0x%x, vpn1=0x%x, vpn0=0x%x\n", vpn2, vpn1, vpn0);
    if (page_list != NULL)
        logging(LOG_VV, "mm", "... page_list=0x%lx\n", (uint64_t) page_list);

    // find level-1 pgtable
    if (!(pt2[vpn2] & _PAGE_PRESENT)) {
        // alloc a new second-level page directory
        page_t *tmp = alloc_page1();
        if (page_list != NULL)
            list_insert(page_list, &tmp->list);
        uintptr_t page = tmp->kva;
        set_pfn(&pt2[vpn2], kva2pa(page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pt2[vpn2], _PAGE_PRESENT);
        clear_pgdir(page);
        pt1 = (PTE *) page;
    } else {
        pt1 = (PTE *) pa2kva(get_pa(pt2[vpn2]));
    }

    logging(LOG_VV, "mm", "... level-1 pgtable at 0x%lx\n", (uint64_t) pt1);

    PTE *pte;
    if (level == 1) {
        // find pte
        pte = &pt1[vpn1];
    } else {
        // find level-0 pgtable
        if (!(pt1[vpn1] & _PAGE_PRESENT)) {
            // alloc a new second-level page directory
            page_t *tmp = alloc_page1();
            if (page_list != NULL)
                list_insert(page_list, &tmp->list);
            uintptr_t page = tmp->kva;
            set_pfn(&pt1[vpn1], kva2pa(page) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pt1[vpn1], _PAGE_PRESENT);
            clear_pgdir(page);
            pt0 = (PTE *) page;
        } else {
            pt0 = (PTE *) pa2kva(get_pa(pt1[vpn1]));
        }

        logging(LOG_VV, "mm", "... level-0 pgtable at 0x%lx\n", (uint64_t) pt0);

        // find pte
        pte = &pt0[vpn0];
    }

    logging(LOG_VERBOSE, "mm", "... pte at 0x%lx\n", (uint64_t) pte);
    return pte;
}

uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb) {
    // NULL
    if (va == 0) {
        logging(LOG_ERROR, "mm", "alloc page for addr 0x0 is prohibited\n");
        return 0;
    }

    list_node_t *page_list = get_page_list(pcb);
    PTE *pte = map_page(va, pcb->pgdir, &pcb->page_list, 0);

    // allocate a new page for va
#ifdef S_CORE
    uintptr_t page = allocLargePage(1);
#else
    page_t *tmp;
    if (remaining_pf == 0) {
        tmp = (page_t *) kmalloc(sizeof(page_t));
        tmp->kva = swap_out();
        list_init(&tmp->list);
        list_init(&tmp->onmem);
        memset((void *) tmp->kva, 0, PAGE_SIZE);
    } else {
        remaining_pf --;
        tmp = alloc_page1();
    }
    tmp->tp = PAGE_USER;
    tmp->swap = NULL;
    list_insert(onmem_list.prev, &tmp->onmem);
    list_insert(page_list, &tmp->list);
    tmp->owner = pcb;
    tmp->va = va & ~(PAGE_SIZE - 1);
    uintptr_t page = tmp->kva;
#endif

    logging(LOG_DEBUG, "mm", "... allocated page at 0x%lx\n", (uint64_t) page);

    // set pgtable
    set_pfn(pte, kva2pa(page) >> NORMAL_PAGE_SHIFT);
    set_attribute(pte, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);

    return page;
}
