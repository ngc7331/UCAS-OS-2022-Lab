#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stdint.h>
typedef int32_t pid_t;

/* sleep */
void sys_sleep(uint32_t time);

/* yield */
void sys_yield(void);

/* print */
void sys_write(char *buff);

/* screen */
void sys_move_cursor(int x, int y);
void sys_reflush(void);
void sys_clear(void);
void sys_move_cursor_r(int x, int y);
void sys_set_scroll_base(int base);

/* time */
long sys_get_timebase(void);
long sys_get_tick(void);

/* lock */
int sys_mutex_init(int key);
void sys_mutex_acquire(int mutex_idx);
void sys_mutex_release(int mutex_idx);

/* show task */
void sys_show_task(void);

/* thread */
pid_t sys_thread_create(uint64_t entrypoint, void *arg);
void sys_thread_join(pid_t tid, void **retval);
void sys_thread_exit(void *retval);

/* ps, getchar */
void sys_ps(void);
int sys_getchar(void);

// #define S_CORE

/* exec */
#ifdef S_CORE
// S-core
pid_t sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
// A/C-core
pid_t sys_exec(char *name, int argc, char **argv);
#endif

/* exit, kill, waitpid, getpid */
void sys_exit(void);
int sys_kill(pid_t pid);
int sys_waitpid(pid_t pid);
pid_t sys_getpid();

/* barrier */
int  sys_barrier_init(int key, int goal);
void sys_barrier_wait(int bar_idx);
void sys_barrier_destroy(int bar_idx);

/* condition */
int sys_condition_init(int key);
void sys_condition_wait(int cond_idx, int mutex_idx);
void sys_condition_signal(int cond_idx);
void sys_condition_broadcast(int cond_idx);
void sys_condition_destroy(int cond_idx);

/* mailbox */
int sys_mbox_open(char * name);
void sys_mbox_close(int mbox_id);
int sys_mbox_send(int mbox_idx, void *msg, int msg_length);
int sys_mbox_recv(int mbox_idx, void *msg, int msg_length);

/* smp */
void sys_taskset(pid_t pid, unsigned mask);

#endif
