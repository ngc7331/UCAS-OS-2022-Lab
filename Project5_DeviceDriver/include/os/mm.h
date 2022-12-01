/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>
#include <os/sched.h>
#include <os/list.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+4*PAGE_SIZE)

typedef struct {
    unsigned int pa; // block id
    list_node_t list;
} swap_t;

typedef struct {
    ptr_t kva;
    ptr_t va;
    list_node_t list;
    swap_t *swap;
    list_node_t onmem;
    pcb_t *owner;
    enum {
        PAGE_USER,
        PAGE_KERNEL,
        PAGE_SHM
    } tp;
} page_t;

extern list_head onmem_list;

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

ptr_t allocPage(int numPage);
page_t *alloc_page1(void);
void free_page1(page_t *page);

void do_garbage_collector(void);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

void *kmalloc(size_t size);
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
list_node_t *get_page_list(pcb_t *pcb);
PTE *map_page(uintptr_t va, uint64_t pgdir, list_node_t *page_list, int level);
uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb);

// swap
void free_swap1(swap_t *swap);
uintptr_t swap_out();
void swap_in(page_t *page, uintptr_t kva);
page_t *check_and_swap(pcb_t *pcb, uintptr_t va);

// shm_page
#define SHM_PAGE_MAX_REF 16
typedef struct {
    int key;
    page_t *page;
    struct {
        // identifier
        pid_t pid;
        // va for user
        uint64_t va;
    } map[SHM_PAGE_MAX_REF];
    int ref;
} shm_page_t;

void init_shm_pages();
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);

// snapshot
uint64_t do_snapshot(uint64_t va);
uint64_t do_getpa(uint64_t va);

#endif /* MM_H */
