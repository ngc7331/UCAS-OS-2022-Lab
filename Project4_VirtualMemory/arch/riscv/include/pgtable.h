#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu
#define _PAGE_CTRL_MASK ((1lu << _PAGE_PFN_SHIFT) - 1)
#define _PAGE_PPN_LEN 44lu
#define _PAGE_PPN_MASK ((1lu << _PAGE_PPN_LEN) - 1)

#define VA_MASK ((1lu << 39) - 1)

#define PPN_BITS 9lu
#define VPN_MASK ((1lu << PPN_BITS) - 1)
#define NUM_PTE_ENTRY (1 << PPN_BITS)

#define KVA_PREFIX 0xFFFFFFC000000000UL

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    return kva & ~KVA_PREFIX;
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    return pa | KVA_PREFIX;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    // PPN = entry[10:54), addr = PPN << 12
    return (entry >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    return entry >> _PAGE_PFN_SHIFT;
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    *entry &= ~(_PAGE_PPN_MASK << _PAGE_PFN_SHIFT);
    *entry |= (pfn & _PAGE_PPN_MASK) << _PAGE_PFN_SHIFT;
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    return entry & mask & _PAGE_CTRL_MASK;
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    *entry &= ~_PAGE_CTRL_MASK;
    *entry |= bits & _PAGE_CTRL_MASK;
}

static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    PTE *p = (PTE *) pgdir_addr;
    for (int i=0; i<NUM_PTE_ENTRY; i++)
        *p++ = 0;
}

static inline int pte_is_valid(PTE entry) {
    return entry & _PAGE_PRESENT;
}

static inline int pte_is_leaf(PTE entry) {
    return entry & (_PAGE_EXEC | _PAGE_WRITE | _PAGE_READ);
}

/* query the page table stored in pgdir_va to obtain the page table entry
 * or the physical address corresponding to the virtual address va.
 */
static inline PTE *get_pte_of(uintptr_t va, uintptr_t pgdir_va) {
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;

    // query level-2 pgtable
    PTE *pt2 = (PTE *) pgdir_va;
    if (!pte_is_valid(pt2[vpn2])) // not present
        return NULL;
    if (pte_is_leaf(pt2[vpn2])) // is leaf
        return &pt2[vpn2];

    // query level-1 pgtable
    PTE *pt1 = (PTE *) pa2kva(get_pa(pt2[vpn2]));
    if (!pte_is_valid(pt1[vpn1])) // not present
        return NULL;
    if (pte_is_leaf(pt1[vpn1])) // is leaf
        return &pt1[vpn1];

    // query level-0 pgtable
    PTE *pt0 = (PTE *) pa2kva(get_pa(pt1[vpn1]));
    if (!pte_is_valid(pt0[vpn0])) // not present
        return NULL;
    return &pt0[vpn0]; // must be leaf in SV39 mode
}

static inline uintptr_t get_kva_of(uintptr_t va, uintptr_t pgdir_va) {
    return pa2kva(get_pa(*get_pte_of(va, pgdir_va)));
}

static inline uint64_t getvpn2(uint64_t va) {
    return va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
}

static inline uint64_t getvpn1(uint64_t va) {
    return (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
}

static inline uint64_t getvpn0(uint64_t va) {
    return (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
}

#endif  // PGTABLE_H
