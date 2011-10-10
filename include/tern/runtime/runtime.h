/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_H
#define __TERN_COMMON_RUNTIME_H

#include <pthread.h>
#include <semaphore.h>

namespace tern {

struct Runtime {
  virtual void progBegin() {}
  virtual void progEnd() {}
  virtual void symbolic(unsigned insid, void *addr,
                        int nbytes, const char *name) {}
  virtual void threadBegin() {}
  virtual void threadEnd(unsigned insid) {}

  // thread
  virtual int pthreadCreate(unsigned insid, pthread_t *th, pthread_attr_t *a,
                            void *(*func)(void*), void *arg) = 0;
  virtual int pthreadJoin(unsigned insid, pthread_t th, void **retval) = 0;
  // no pthreadExit since it's handled via threadEnd()

  // mutex
  virtual int pthreadMutexLock(unsigned insid, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTryLock(unsigned insid, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTimedLock(unsigned insid, pthread_mutex_t *mu,
                                    const struct timespec *abstime) = 0;
  virtual int pthreadMutexUnlock(unsigned insid, pthread_mutex_t *mutex) = 0;

  // cond var
  virtual int pthreadCondWait(unsigned insid, pthread_cond_t *cv,
                              pthread_mutex_t *mu) = 0;
  virtual int pthreadCondTimedWait(unsigned insid, pthread_cond_t *cv,
                                   pthread_mutex_t *mu,
                                   const struct timespec *abstime) = 0;
  virtual int pthreadCondSignal(unsigned insid, pthread_cond_t *cv) = 0;
  virtual int pthreadCondBroadcast(unsigned insid, pthread_cond_t *cv) = 0;

  // barrier
  virtual int pthreadBarrierInit(unsigned insid, pthread_barrier_t *barrier,
                                 unsigned count) = 0;
  virtual int pthreadBarrierWait(unsigned insid,
                                 pthread_barrier_t *barrier) = 0;
  virtual int pthreadBarrierDestroy(unsigned insid,
                                    pthread_barrier_t *barrier) = 0;

  // semaphore
  virtual int semWait(unsigned insid, sem_t *sem) = 0;
  virtual int semTryWait(unsigned insid, sem_t *sem) = 0;
  virtual int semTimedWait(unsigned insid, sem_t *sem,
                           const struct timespec *abstime) = 0;
  virtual int semPost(unsigned insid, sem_t *sem) = 0;


  /// Installs a runtime as @the.  A runtime implementation must
  /// re-implement this function to install itself
  static void install();
  static Runtime *the;
};

extern void InstallRuntime();

}

#endif
