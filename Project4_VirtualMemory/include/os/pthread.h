#ifndef INCLUDE_THREAD_H_
#define INCLUDE_THREAD_H_

#include <type.h>

pcb_t *get_parent(pid_t pid);

pid_t pthread_create(uint64_t entrypoint, void *arg);
int pthread_join(pid_t tid);
void pthread_exit();

#endif
