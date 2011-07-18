/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#ifndef __TERN_RECORDER_RUNTIME_H
#define __TERN_RECORDER_RUNTIME_H

#include <tr1/unordered_map>
#include "log.h"
#include "recscheduler.h"
#include "common/runtime/runtime.h"
#include "common/runtime/helper.h"

namespace tern {

struct barrier_t {
  int count;    // barrier count
  int narrived; // number of threads arrived at the barrier
};
typedef std::tr1::unordered_map<pthread_barrier_t*, barrier_t> barrier_map;

template <typename _Scheduler>
struct RecorderRT: public Runtime, public _Scheduler {

  void progBegin(void);
  void progEnd(void);
  void threadBegin(void);
  void threadEnd(void);

  // thread
  int pthreadCreate(unsigned insid, pthread_t *thread,  pthread_attr_t *attr,
                    void *(*thread_func)(void*), void *arg);
  int pthreadJoin(unsigned insid, pthread_t th, void **thread_return);

  // mutex
  int pthreadMutexLock(unsigned insid, pthread_mutex_t *mutex);
  int pthreadMutexTimedLock(unsigned insid, pthread_mutex_t *mutex,
                            const struct timespec *abstime);
  int pthreadMutexTryLock(unsigned insid, pthread_mutex_t *mutex);

  int pthreadMutexUnlock(unsigned insid, pthread_mutex_t *mutex);

  // cond var
  int pthreadCondWait(unsigned insid, pthread_cond_t *cv, pthread_mutex_t *mu);
  int pthreadCondTimedWait(unsigned insid, pthread_cond_t *cv,
                           pthread_mutex_t *mu, const struct timespec *abstime);
  int pthreadCondSignal(unsigned insid, pthread_cond_t *cv);
  int pthreadCondBroadcast(unsigned insid, pthread_cond_t *cv);

  // barrier
  int pthreadBarrierInit(unsigned insid, pthread_barrier_t *barrier, unsigned count);
  int pthreadBarrierWait(unsigned insid, pthread_barrier_t *barrier);
  int pthreadBarrierDestroy(unsigned insid, pthread_barrier_t *barrier);

  // semaphore
  int semWait(unsigned insid, sem_t *sem);
  int semTryWait(unsigned insid, sem_t *sem);
  int semTimedWait(unsigned insid, sem_t *sem, const struct timespec *abstime);
  int semPost(unsigned insid, sem_t *sem);

  void symbolic(unsigned insid, void *addr, int nbytes, const char *name);

  RecorderRT(): _Scheduler(pthread_self()) {
    int ret = sem_init(&thread_create_sem, 0, 1); // main thread
    assert(!ret && "can't initialize semaphore!");
  }

protected:

  void pthreadMutexLockHelper(pthread_mutex_t *mutex);

  /// for each pthread barrier, track the count of the number and number
  /// of threads arrived at the barrier
  barrier_map barriers;

  //Logger logger;
  /// need this semaphore to assign tid deterministically; see
  /// pthreadCreate() and threadBegin()
  sem_t thread_create_sem;
};

} // namespace tern

#endif
