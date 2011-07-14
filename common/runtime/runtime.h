/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_H
#define __TERN_COMMON_RUNTIME_H

#include <pthread.h>

namespace tern {

struct Runtime {
  virtual void progBegin() {}
  virtual void progEnd() {}
  virtual void symbolic(void *addr, int nbytes, const char *name) {}
  virtual void threadBegin() {}
  virtual void threadEnd() {}

  // thread
  virtual int pthreadCreate(int insid, pthread_t *th, pthread_attr_t *a,
                                  void *(*func)(void*), void *arg) = 0;
  virtual int pthreadJoin(int insid, pthread_t th, void **retval) = 0;
  // no pthreadExit since it's handled via threadEnd()

  // mutex
  virtual int pthreadMutexLock(int insid, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTryLock(int insid, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTimedLock(int insid, pthread_mutex_t *mu,
                                    const struct timespec *abstime) = 0;
  virtual int pthreadMutexUnlock(int insid, pthread_mutex_t *mutex) = 0;

  // cond var
  virtual int pthreadCondWait(int insid, pthread_cond_t *cv,
                              pthread_mutex_t *mu) = 0;
  virtual int pthreadCondTimedWait(int insid, pthread_cond_t *cv,
                                   pthread_mutex_t *mu,
                                   const struct timespec *abstime) = 0;
  virtual int pthreadCondSignal(int insid, pthread_cond_t *cv) = 0;
  virtual int pthreadCondBroadcast(int insid, pthread_cond_t *cv) = 0;

  /// Installs a runtime as @the.  A runtime implementation must
  /// re-implement this function to install itself
  static void install();
  static Runtime *the;
};

}

#endif
