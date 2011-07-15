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
  int pthreadCreate(int insid, pthread_t *thread,  pthread_attr_t *attr,
                    void *(*thread_func)(void*), void *arg);
  int pthreadJoin(int insid, pthread_t th, void **thread_return);

  // mutex
  int pthreadMutexLock(int insid, pthread_mutex_t *mutex);
  int pthreadMutexTimedLock(int insid, pthread_mutex_t *mutex,
                            const struct timespec *abstime);
  int pthreadMutexTryLock(int insid, pthread_mutex_t *mutex);

  int pthreadMutexUnlock(int insid, pthread_mutex_t *mutex);

  // cond var
  int pthreadCondWait(int insid, pthread_cond_t *cv, pthread_mutex_t *mu);
  int pthreadCondTimedWait(int insid, pthread_cond_t *cv,
                           pthread_mutex_t *mu, const struct timespec *abstime);
  int pthreadCondSignal(int insid, pthread_cond_t *cv);
  int pthreadCondBroadcast(int insid, pthread_cond_t *cv);

  // barrier
  int pthreadBarrierInit(int insid, pthread_barrier_t *barrier, unsigned count);
  int pthreadBarrierWait(int insid, pthread_barrier_t *barrier);
  int pthreadBarrierDestroy(int insid, pthread_barrier_t *barrier);

  // semaphore
  int semWait(int insid, sem_t *sem);
  int semTryWait(int insid, sem_t *sem);
  int semTimedWait(int insid, sem_t *sem, const struct timespec *abstime);
  int semPost(int insid, sem_t *sem);

  void symbolic(void *addr, int nbytes, const char *name);

  /// tick once for each sync event
  /// this is just the turn count, so put in scheduler
  int tick() { return nsync++; }

  RecorderRT() {
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
  int nsync;
};

} // namespace tern

#endif
