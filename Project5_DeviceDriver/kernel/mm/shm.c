#include <os/mm.h>
#include <os/smp.h>
#include <os/string.h>
#include <printk.h>

#define SHM_PAGE_BASE 0x80000000
#define SHM_PAGE_LIM ((SHM_PAGE_BASE) + 0x1000 * PAGE_SIZE)
#define SHM_PAGE_NUM 64

shm_page_t shm_pages[SHM_PAGE_NUM];

// NOTE: shm page is not allowed to swap out

void init_shm_pages() {
    for (int i=0; i<SHM_PAGE_NUM; i++) {
        shm_pages[i].ref = 0;
        shm_pages[i].page = NULL;
    }
}

uintptr_t shm_page_get(int key)
{
    int cid = get_current_cpu_id();
    // find an available va for user
    uintptr_t va = 0;
    for (uintptr_t i=SHM_PAGE_BASE; i<SHM_PAGE_LIM; i+=PAGE_SIZE) {
        PTE *pte = get_pte_of(i, current_running[cid]->pgdir, 4);
        if (pte == NULL || *pte == 0) {
            // not allocated(pte!=NULL) and not on disk(*pte!=0)
            va = i;
            break;
        }
    }
    if (va == 0) {
        logging(LOG_ERROR, "shm", "failed to find available va for shm\n");
        do_exit();
    }
    // find / allocate a shm page for key
    int idx = -1;
    for (int i=0; i<SHM_PAGE_NUM; i++) {
        if (shm_pages[i].ref > 0 && shm_pages[i].key == key) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        for (int i=0; i<SHM_PAGE_NUM; i++) {
            if (shm_pages[i].ref == 0) {
                idx = i;
                break;
            }
        }
    }
    // check if allocate successfully
    if (idx < 0) {
        logging(LOG_ERROR, "shm", "failed to find / allocate a shm for key=%d\n", key);
        do_exit();
    }
    if (shm_pages[idx].ref >= SHM_PAGE_MAX_REF) {
        logging(LOG_ERROR, "shm", "maximum references exceeded for shm[%d]\n", idx);
        do_exit();
    }
    // record key
    shm_pages[idx].key = key;
    // ensure page frame exists
    if (shm_pages[idx].page == NULL) {
        shm_pages[idx].page = alloc_page1();
        shm_pages[idx].page->tp = PAGE_SHM;
    }
    // record in map
    shm_pages[idx].map[shm_pages[idx].ref].pid = current_running[cid]->pid;
    shm_pages[idx].map[shm_pages[idx].ref].va = va;
    // add ref
    shm_pages[idx].ref ++;
    // map page
    PTE *pte = map_page(va, current_running[cid]->pgdir, NULL);
    // set pgtable
    set_pfn(pte, kva2pa(shm_pages[idx].page->kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(pte, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);

    logging(LOG_INFO, "shm", "%d.%s attach shm[%d] with key=%d\n",
            current_running[cid]->pid, current_running[cid]->name, idx, key);
    logging(LOG_DEBUG, "shm", "... kva=0x%x%x, va=0x%x%x\n",
            shm_pages[idx].page->kva>>32, shm_pages[idx].page->kva, va>>32, va);

    return va;
}

void shm_page_dt(uintptr_t addr)
{
    int cid = get_current_cpu_id();
    int idx=-1, mapidx=-1;
    for (int i=0; i<SHM_PAGE_NUM; i++) {
        if (shm_pages[i].ref == 0)
            continue;
        for (int j=0; j<SHM_PAGE_MAX_REF; j++) {
            if (shm_pages[i].map[j].pid == current_running[cid]->pid &&
                shm_pages[i].map[j].va == addr) {
                idx = i, mapidx = j;
                break;
            }
        }
        if (idx >= 0)
            break;
    }
    // check if found
    if (idx < 0 || mapidx < 0) {
        logging(LOG_ERROR, "shm", "failed to detach: no shm found\n");
    }
    // sub ref
    shm_pages[idx].ref --;
    // remove from map
    for (int j=mapidx; j<shm_pages[idx].ref; j++) {
        shm_pages[idx].map[j] = shm_pages[idx].map[j+1];
    }
    // clear pgtable
    PTE *pte = get_pte_of(addr, current_running[cid]->pgdir, 0);
    *pte = 0;
    // free page if ref == 0
    if (shm_pages[idx].ref == 0) {
        free_page1(shm_pages[idx].page);
        shm_pages[idx].page = NULL;
    }

    logging(LOG_INFO, "shm", "%d.%s detach shm[%d]\n",
            current_running[cid]->pid, current_running[cid]->name, idx);
}
