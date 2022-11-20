#include <pthread.h>
#include <unistd.h>

void pthread_create(pthread_t *thread,
                   void (*start_routine)(void*),
                   void *arg) {
    *thread = sys_pthread_create((uint64_t) start_routine, arg);
}

int pthread_join(pthread_t thread) {
    return sys_pthread_join(thread);
}
