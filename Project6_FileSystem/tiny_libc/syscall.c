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

void sys_clear(void) {
    /* call invoke_syscall to implement sys_clear */
    invoke_syscall(SYSCALL_CLEAR, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor_r(int x, int y) {
    // move cursor using relative coordinates
    invoke_syscall(SYSCALL_CURSOR_R, x, y, IGNORE, IGNORE, IGNORE);
}

void sys_set_scroll_base(int base) {
    // set scroll down base
    invoke_syscall(SYSCALL_SET_SC_BASE, base, IGNORE, IGNORE, IGNORE, IGNORE);
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

void sys_show_task(void)
{
    /* call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_SHOW_TASK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
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

pid_t sys_pthread_create(uint64_t entrypoint, void *arg) {
    return invoke_syscall(SYSCALL_PTHREAD_CREATE, entrypoint, (long) arg, IGNORE, IGNORE, IGNORE);
}

int sys_pthread_join(pid_t tid) {
    return invoke_syscall(SYSCALL_PTHREAD_JOIN, tid, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_pthread_exit() {
    invoke_syscall(SYSCALL_PTHREAD_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

#ifdef S_CORE_P3
// S-core
pid_t sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* call invoke_syscall to implement sys_exec for P3 S_CORE */
    return invoke_syscall(SYSCALL_EXEC, id, argc, arg0, arg1, arg2);
}
#else
// A/C-core
pid_t sys_exec(char *name, int argc, char **argv)
{
    /* call invoke_syscall to implement sys_exec */
    return invoke_syscall(SYSCALL_EXEC, (long) name, argc, (long) argv, IGNORE, IGNORE);
}
#endif

void sys_exit(void)
{
    /* call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_kill(pid_t pid)
{
    /* call invoke_syscall to implement sys_kill */
    return invoke_syscall(SYSCALL_KILL, pid, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_waitpid(pid_t pid)
{
    /* call invoke_syscall to implement sys_waitpid */
    return invoke_syscall(SYSCALL_WAITPID, pid, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_ps(int mode)
{
    /* call invoke_syscall to implement sys_ps */
    invoke_syscall(SYSCALL_PS, mode, IGNORE, IGNORE, IGNORE, IGNORE);
}

pid_t sys_getpid()
{
    /* call invoke_syscall to implement sys_getpid */
    return invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_getchar(void)
{
    /* call invoke_syscall to implement sys_getchar */
    return invoke_syscall(SYSCALL_READCH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_barrier_init(int key, int goal)
{
    /* call invoke_syscall to implement sys_barrier_init */
    return invoke_syscall(SYSCALL_BARR_INIT, key, goal, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_wait(int bar_idx)
{
    /* call invoke_syscall to implement sys_barrier_wait */
    invoke_syscall(SYSCALL_BARR_WAIT, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_destroy(int bar_idx)
{
    /* call invoke_syscall to implement sys_barrier_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_condition_init(int key)
{
    /* call invoke_syscall to implement sys_condition_init */
    return invoke_syscall(SYSCALL_COND_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* call invoke_syscall to implement sys_condition_wait */
    invoke_syscall(SYSCALL_COND_WAIT, cond_idx, mutex_idx, IGNORE, IGNORE, IGNORE);
}

void sys_condition_signal(int cond_idx)
{
    /* call invoke_syscall to implement sys_condition_signal */
    invoke_syscall(SYSCALL_COND_SIGNAL, cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_broadcast(int cond_idx)
{
    /* call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall(SYSCALL_COND_BROADCAST, cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_destroy(int cond_idx)
{
    /* call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall(SYSCALL_COND_DESTROY, cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_open(char * name)
{
    /* call invoke_syscall to implement sys_mbox_open */
    return invoke_syscall(SYSCALL_MBOX_OPEN, (long) name, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mbox_close(int mbox_idx)
{
    /* call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE, mbox_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* call invoke_syscall to implement sys_mbox_send */
    return invoke_syscall(SYSCALL_MBOX_SEND, mbox_idx, (long) msg, msg_length, IGNORE, IGNORE);
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* call invoke_syscall to implement sys_mbox_recv */
    return invoke_syscall(SYSCALL_MBOX_RECV, mbox_idx, (long) msg, msg_length, IGNORE, IGNORE);
}

void sys_taskset(pid_t pid, unsigned mask) {
    invoke_syscall(SYSCALL_TASKSET, pid, mask, IGNORE, IGNORE, IGNORE);
}

void *sys_shmpageget(int key) {
    return (void *) invoke_syscall(SYSCALL_SHM_GET, key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_shmpagedt(void *addr) {
    invoke_syscall(SYSCALL_SHM_DT, (long) addr, IGNORE, IGNORE, IGNORE, IGNORE);
}

uint64_t sys_snapshot(uint64_t va) {
    return invoke_syscall(SYSCALL_SNAPSHOT, va, IGNORE, IGNORE, IGNORE, IGNORE);
}

uint64_t sys_getpa(uint64_t va) {
    return invoke_syscall(SYSCALL_GETPA, va, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_net_send(void *txpacket, int length) {
    return invoke_syscall(SYSCALL_NET_SEND, (long) txpacket, length, IGNORE, IGNORE, IGNORE);
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens) {
    return invoke_syscall(SYSCALL_NET_RECV, (long) rxbuffer, pkt_num, (long) pkt_lens, IGNORE, IGNORE);
}

int sys_mkfs(void) {
    return invoke_syscall(SYSCALL_FS_MKFS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_statfs(void){
    return invoke_syscall(SYSCALL_FS_STATFS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_cd(char *path) {
    return invoke_syscall(SYSCALL_FS_CD, (long) path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mkdir(char *path) {
    return invoke_syscall(SYSCALL_FS_MKDIR, (long) path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_rmdir(char *path) {
    return invoke_syscall(SYSCALL_FS_RMDIR, (long) path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_ls(char *path, int option) {
    // Note: argument 'option' serves for 'ls -l' in A-core
    return invoke_syscall(SYSCALL_FS_LS, (long) path, option, IGNORE, IGNORE, IGNORE);
}

int sys_touch(char *path) {
    return invoke_syscall(SYSCALL_FS_TOUCH, (long) path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_cat(char *path) {
    return invoke_syscall(SYSCALL_FS_CAT, (long) path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_fopen(char *path, int mode) {
    return invoke_syscall(SYSCALL_FS_FOPEN, (long) path, mode, IGNORE, IGNORE, IGNORE);
}

int sys_fread(int fd, char *buff, int length) {
    return invoke_syscall(SYSCALL_FS_FREAD, fd, (long) buff, length, IGNORE, IGNORE);
}

int sys_fwrite(int fd, char *buff, int length) {
    return invoke_syscall(SYSCALL_FS_FWRITE, fd, (long) buff, length, IGNORE, IGNORE);
}

int sys_fclose(int fd) {
    return invoke_syscall(SYSCALL_FS_FCLOSE, fd, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_ln(char *src_path, char *dst_path) {
    return invoke_syscall(SYSCALL_FS_LN, (long) src_path, (long) dst_path, IGNORE, IGNORE, IGNORE);
}

int sys_rm(char *path) {
    return invoke_syscall(SYSCALL_FS_RM, (long) path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_lseek(int fd, int offset, int whence) {
    return invoke_syscall(SYSCALL_FS_LSEEK, fd, offset, whence, IGNORE, IGNORE);
}
