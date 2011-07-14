/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#ifndef __TERN_RECORDER_RUNTIME_H
#define __TERN_RECORDER_RUNTIME_H

#include "log.h"
#include "recscheduler.h"
#include "common/runtime/runtime.h"
#include "common/runtime/helper.h"

namespace tern {

template <typename _Sched>
struct RecorderRuntime: public Runtime, public _Sched {

  void progBegin(void);
  void progEnd(void);
  void threadBegin(void);
  void threadEnd(void);

  int pthreadCreate(int insid, pthread_t *thread,  pthread_attr_t *attr,
                    void *(*thread_func)(void*), void *arg);
  int pthreadJoin(int insid, pthread_t th, void **thread_return);

  int pthreadMutexLock(int insid, pthread_mutex_t *mutex);
  int pthreadMutexTimedLock(int insid, pthread_mutex_t *mutex,
                            const struct timespec *abstime);
  int pthreadMutexTryLock(int insid, pthread_mutex_t *mutex);

  int pthreadMutexUnlock(int insid, pthread_mutex_t *mutex);

  int pthreadCondWait(int insid, pthread_cond_t *cv, pthread_mutex_t *mu);
  int pthreadCondTimedWait(int insid, pthread_cond_t *cv,
                           pthread_mutex_t *mu, const struct timespec *abstime);
  int pthreadCondSignal(int insid, pthread_cond_t *cv);
  int pthreadCondBroadcast(int insid, pthread_cond_t *cv);

  void symbolic(void *addr, int nbytes, const char *name);

  /// tick once for each sync event
  int tick() { return nsync++; }

  RecorderRuntime() {
    int ret = sem_init(&thread_create_sem, 0, 1); // main thread
    assert(!ret && "can't initialize semaphore!");
  }

protected:

  void pthreadMutexLockHelper(pthread_mutex_t *mutex);

  //Logger logger;
  /// need this semaphore to assign tid deterministically; see
  /// pthreadCreate() and threadBegin()
  sem_t thread_create_sem;
  int nsync;
};

} // namespace tern

#endif
