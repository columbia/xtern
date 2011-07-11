/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_RUNTIME_HELPER_H
#define __TERN_COMMON_RUNTIME_HELPER_H

#include <pthread.h>
#include <tr1/unordered_map>

extern "C" {

  /* helper function used by tern runtimes.  It ensures a newly created
   * thread calls tern_task_begin() and tern_pthread_exit() */
  int __tern_pthread_create(pthread_t *thread,  pthread_attr_t *attr,
                          void* (*start_routine)(void*), void *arg);

  /* tern inserts this helper function to the beginning of a program
   * (either as the first static constructor, or the first instruction in
   * main()).  This function schedules __tern_prog_end() and calls
   * tern_prog_begin() and tern_thread_begin() (for the main thread) */
  void __tern_prog_begin(void);

  /* calls tern_thread_end() (for the main thread) and tern_prog_end() */
  void __tern_prog_end(void);

  /* similar to tern_symbolic() except inserted by tern and checks for
   * valid arguments */
  void __tern_symbolic(void *addr, int nbytes, const char *symname);

  /* mainly for marking arguments of main as symbolic */
  void __tern_symbolic_argv(int argc, char **argv);

}

namespace tern {

struct tid_manager {
  enum {InvalidTid = -1};

  typedef std::tr1::unordered_map<pthread_t, int> pthread_to_tern_map;
  typedef std::tr1::unordered_map<int, pthread_t> tern_to_pthread_map;

  tid_manager(): nthread(1) {}

  void on_pthread_create(pthread_t pthread_tid);
  pthread_t get_pthread_tid(int tern_tid);
  int get_tern_tid(pthread_t pthread_tid);

  pthread_to_tern_map p_t_map;
  tern_to_pthread_map t_p_map;
  int nthread;
};

extern __thread int tid; // tern tid for current thread

}

int tern_self(void);

#endif
