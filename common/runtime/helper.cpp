/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "helper.h"
#include "runtime-interface.h"

typedef void * (*thread_func_t)(void*);

static void *__tern_thread_func(void *arg) {

  void **args;
  void *ret_val;
  thread_func_t user_thread_func;
  void *user_thread_arg;

  args = (void**)arg;
  user_thread_func = (thread_func_t)((intptr_t)args[0]);
  user_thread_arg = args[1];
  // free arg before calling user_thread_func, as it may not return (i.e.,
  // call pthread_exit())
  delete[] (void**)arg;

  tern_task_begin(NULL, 0, NULL);
  ret_val = user_thread_func(user_thread_arg);
  tern_pthread_exit(-1, ret_val); // calls tern_task_end() and pthread_exit()
  assert(0 && "unreachable");
}

int __tern_pthread_create(pthread_t *thread,  pthread_attr_t *attr,
                          thread_func_t user_thread_func,
                          void *user_thread_arg) {
  void **args;

  // use heap because stack of this func may be deallocated before the
  // created thread reads the args
  args = new void*[2];
  args[0] = (void*)(intptr_t)user_thread_func;
  args[1] = user_thread_arg;

  return pthread_create(thread, attr, __tern_thread_func, (void*)args);
}

void __tern_prog_begin(void) {
  atexit(tern_prog_end);
  tern_prog_begin();
}

void __tern_symbolic(void *addr, int nbytes, const char *symname) {
  if(nbytes <= 0 || !addr)
    return;
  tern_symbolic(addr, nbytes, symname);
}

void __tern_symbolic_argv(int argc, char **argv) {
  char arg[32];
  for(int i=0; i<argc; ++i) {
    sprintf(arg, "arg%d\n", i);
    tern_symbolic(argv[i], strlen(argv[i])+1, arg);
  }
}

