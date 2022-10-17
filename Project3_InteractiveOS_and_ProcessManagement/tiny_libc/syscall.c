#include <syscall.h>
#include <stdint.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* implement invoke_syscall via inline assembly */
    long retval = 0;
    asm volatile(
        "add a7, %1, zero\n\r"
        "add a0, %2, zero\n\r"
        "add a1, %3, zero\n\r"
        "add a2, %4, zero\n\r"
        "add a3, %5, zero\n\r"
        "add a4, %6, zero\n\r"
        "ecall\n\r"
        "add %0, a0, zero\n\r"
        : "=r"(retval)
        : "r"(sysno), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)
        : "a0", "a1", "a2", "a3", "a4", "a7"
    );
    // asm volatile(
    //     "add a7, a0, zero\n\r"
    //     "add a0, a1, zero\n\r"
    //     "add a1, a2, zero\n\r"
    //     "add a2, a3, zero\n\r"
    //     "add a3, a4, zero\n\r"
    //     "add a4, a5, zero\n\r"
    //     "ecall\n\r"
    //     "add %0, a0, zero\n\r"
    //     : "=r"(retval)
    //     :
    //     : "a0", "a1", "a2", "a3", "a4", "a5", "a7"
    // );
    return retval;
}

void sys_yield(void)
{
    /* call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y)
{
    /* call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE, IGNORE, IGNORE);
}

void sys_write(char *buff)
{
    /* call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE, (long) buff, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_reflush(void)
{
    /* call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);

}

int sys_mutex_init(int key)
{
    /* call invoke_syscall to implement sys_mutex_init */
    return invoke_syscall(SYSCALL_LOCK_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_acquire(int mutex_idx)
{
    /* call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    /* call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_timebase(void)
{
    /* call invoke_syscall to implement sys_get_timebase */
    return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_tick(void)
{
    /* call invoke_syscall to implement sys_get_tick */
    return invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_sleep(uint32_t time)
{
    /* call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_thread_create(uint64_t entrypoint, void *arg) {
    return invoke_syscall(SYSCALL_THREAD_CREATE, entrypoint, (long) arg, IGNORE, IGNORE, IGNORE);
}

void sys_thread_join(int tid, void **retval) {
    invoke_syscall(SYSCALL_THREAD_JOIN, tid, (long) retval, IGNORE, IGNORE, IGNORE);
}

void sys_thread_exit(void *retval) {
    invoke_syscall(SYSCALL_THREAD_EXIT, (long) retval, IGNORE, IGNORE, IGNORE, IGNORE);
}

// S-core
pid_t sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}
// A/C-core
// pid_t sys_exec(char *name, int argc, char **argv)
// {
//     /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
// }

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
}

void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
}

pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
}

int sys_mbox_open(char * name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
}
