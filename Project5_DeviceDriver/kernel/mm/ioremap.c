#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // map one specific physical region to virtual address
    void *retval = (void *) io_base;

    // map level-1 pagedir if (size % LARGE_PAGE_SIZE == 0) to save memory
    int use_large_page = size % LARGE_PAGE_SIZE == 0;
    unsigned long step = use_large_page ? LARGE_PAGE_SIZE : NORMAL_PAGE_SIZE;

    for (unsigned long i=0; i<size; i+=step) {
        PTE *pte = map_page(io_base, pa2kva(PGDIR_PA), NULL, use_large_page);
        set_pfn(pte, phys_addr >> NORMAL_PAGE_SHIFT);
        set_attribute(pte, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
        io_base += step;
        phys_addr += step;
    }

    // flush hardware since pgtable is modified
    local_flush_tlb_all();

    return retval;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
