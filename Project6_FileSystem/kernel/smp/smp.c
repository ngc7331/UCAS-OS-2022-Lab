#include <atomic.h>
#include <csr.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

spin_lock_t kernel_lock;

void smp_init() {
    spin_lock_init(&kernel_lock);
}

void wakeup_other_hart() {
    send_ipi(NULL);
    // clear sip
    asm volatile(
        "csrw %0, zero\n\r"
        :
        : "I" (CSR_SIP)
    );
}

void lock_kernel() {
    spin_lock_acquire(&kernel_lock);
}

void unlock_kernel() {
    spin_lock_release(&kernel_lock);
}
