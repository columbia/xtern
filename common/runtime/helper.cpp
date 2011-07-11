/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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

  tern_thread_begin(NULL, 0, NULL);
  ret_val = user_thread_func(user_thread_arg);
  tern_pthread_exit(-1, ret_val); // calls tern_task_end() and pthread_exit()
  assert(0 && "unreachable");
}

int __tern_pthread_create(pthread_t *thread,  pthread_attr_t *attr,
                          thread_func_t user_thread_func,
                          void *user_thread_arg) {
  void **args;

  // use heap because stack of this func may be deallocated before the
  // created thread reads the @args
  args = new void*[2];
  args[0] = (void*)(intptr_t)user_thread_func;
  args[1] = user_thread_arg;

  int saved_errno, ret;
  ret = pthread_create(thread, attr, __tern_thread_func, (void*)args);
  if(ret < 0) {
    saved_errno = errno;
    delete[] (void**)args; // clean up memory for @args
    errno = saved_errno;
  }
  return ret;
}

void __tern_prog_begin(void) {
  atexit(__tern_prog_end);
  tern_prog_begin();
  tern_thread_begin(NULL, 0, NULL);
}

void __tern_prog_end (void) {
  tern_thread_end();
  tern_prog_end();
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

namespace tern {

void tid_manager::on_pthread_create(pthread_t pthread_tid) {
  p_t_map[pthread_tid] = nthread;
  t_p_map[nthread] = pthread_tid;
  ++ nthread;
}

pthread_t tid_manager::get_pthread_tid(int tern_tid) {
  tern_to_pthread_map::iterator it = t_p_map.find(tern_tid);
  if(it!=t_p_map.end())
    return it->second;
  return InvalidTid;
}

int tid_manager::get_tern_tid(pthread_t pthread_tid) {
  pthread_to_tern_map::iterator it = p_t_map.find(pthread_tid);
  if(it!=p_t_map.end())
    return it->second;
  return InvalidTid;
}

__thread int tid = 0; // main thread has tid 0

}
