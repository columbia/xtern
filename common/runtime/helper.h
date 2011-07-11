/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_RUNTIME_HELPER_H
#define __TERN_COMMON_RUNTIME_HELPER_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* helper function used by tern runtimes.  It ensures a newly created
   * thread calls tern_task_begin() and tern_pthread_exit() */
  int __tern_pthread_create(pthread_t *thread,  pthread_attr_t *attr,
                          void* (*start_routine)(void*), void *arg);

  /* tern inserts this helper function to the beginning of a program
   * (either as the first static constructor, or the first instruction in
   * main()).  This function schedules tern_prog_end() and calls
   * tern_prog_begin() */
  void __tern_prog_begin(void);

  /* similar to tern_symbolic() except inserted by tern and checks for
   * valid arguments */
  void __tern_symbolic(void *addr, int nbytes, const char *symname);

  /* mainly for marking arguments of main as symbolic */
  void __tern_symbolic_argv(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif
