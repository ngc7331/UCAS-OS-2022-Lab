#ifndef INCLUDE_THREAD_H_
#define INCLUDE_THREAD_H_

#include <type.h>

pid_t thread_create(uint64_t entrypoint, void *arg);
void thread_join(pid_t tid, void **retval);
void thread_exit(void *retval);

#endif
