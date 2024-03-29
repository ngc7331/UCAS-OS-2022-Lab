/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
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

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>

#define NUM_MAX_TASK 128

/* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.
       0    : zero
       1    : ra
       2~4  : sp, gp, tp
       6~7  : t0~t2
       8~9  : s0~s1
       10~17: a0~a7
       18~27: s2~s11
       28~31: t3~t6
    */
    reg_t regs[32];

    /* Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.
     * ra, sp, s0 ~ s11
     * see arch/riscv/include/asm/regs.h
    */
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED,
} task_status_t;

typedef enum {
    TYPE_PROCESS,
    TYPE_THREAD
} pcb_type_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;
    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    /* previous, next pointer */
    list_node_t list;

    /* wait list */
    list_head wait_list;

    /* process id & thread id
     * for TYPE_PROCESS:
     *   pid is valid
     *   tid = last allocated tid + 1
     * for TYPE_THREAD:
     *   pid = parent's pid
     *   tid is valid
     */
    pid_t pid;
    pid_t tid;
    pcb_type_t type;

    /* cpu */
    int cid;
    unsigned mask;

    /* process name */
    char name[32];

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

    /* return value */
    void *retval;

    /* who joined this process / thread
     * should be initialized as NULL
     */
    struct pcb *joined;

} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
extern pcb_t * volatile current_running[2];
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern pcb_t pid0_pcb[2];
extern const ptr_t pid0_stack[2];
extern int pcb_n;

// #define S_CORE

#ifdef S_CORE
pid_t init_pcb(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
pid_t init_pcb(char *name, int argc, char *argv[]);
#endif

extern void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(pcb_t *, list_head *queue);
void do_unblock(list_node_t *);

/* exec exit kill waitpid ps*/
#ifdef S_CORE
pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
pid_t do_exec(char *name, int argc, char *argv[]);
#endif
void do_exit(void);
int do_kill(pid_t pid);
int do_waitpid(pid_t pid);
void do_process_show();
pid_t do_getpid();

void do_taskset(pid_t pid, unsigned mask);

#endif
