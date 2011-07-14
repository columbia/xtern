/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#ifndef __TERN_RECORDER_SCHEDULER_H
#define __TERN_RECORDER_SCHEDULER_H

#include <list>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "common/runtime/scheduler.h"

namespace tern {

struct FCFSScheduler: public Scheduler {
  void getTurn(void) {
    pthread_mutex_lock(&lock);
  }
  void putTurn(void) {
    pthread_mutex_unlock(&lock);
  }
  void wait(void *chan) {
    pthread_mutex_unlock(&lock);
    sched_yield();
  }
  void signal(void *chan) { }

  void threadBegin() {
    getTurn();
    onThreadBegin();
  }

  void threadEnd() {
    onThreadEnd();
    putTurn();
  }

  void threadCreate(pthread_t new_th) {
    onThreadCreate(new_th);
  }

  pthread_mutex_t *getLock() {
    return &lock;
  }

  FCFSScheduler() {
    pthread_mutex_init(&lock, NULL);
  }

protected:
  pthread_mutex_t lock; // protects TidMap data
};

struct RRSchedulerCV: public Scheduler {

  void getTurn(void) {
    pthread_mutex_lock(&lock);
    getTurnNoLock();
    pthread_mutex_unlock(&lock);
  }

  void putTurn(void) {
    int tid = self();
    pthread_mutex_lock(&lock);
    assert(tid == activeq.front());
    activeq.pop_front();
    activeq.push_back(tid);
    tid = activeq.front();
    pthread_cond_signal(&waitcv[tid]);
    pthread_mutex_unlock(&lock);
  }

  /// give up turn & deterministically wait on @chan as the last thread in
  /// @waitq
  void wait(void *chan) {
    int tid = self();
    pthread_mutex_lock(&lock);
    waitq.push_back(tid);
    assert(waitvar[tid] && "thread already waits!");
    waitvar[tid] = chan;
    while(waitvar[tid])
      pthread_cond_wait(&waitcv[tid], &lock);
    pthread_mutex_unlock(&lock);
  }

  /// deterministically wake up the first thread waiting on @chan on the
  /// wait queue; must call with turn held
  void signal(void *chan) {
    pthread_mutex_lock(&lock);
    signalNoLock(chan);
    pthread_mutex_unlock(&lock);
  }

  /// if any thread is waiting on our exit, wake it up, then give up turn
  /// and exit (not putting self back to the tail of activeq)
  void threadEnd() {
    int tid = self();
    pthread_mutex_lock(&lock);
    assert(tid == activeq.front());
    activeq.pop_front();
    signalNoLock((void*)tid);
    pthread_mutex_unlock(&lock);
  }

  void threadBegin() {
    pthread_mutex_lock(&lock);
    onThreadBegin();
    getTurnNoLock();
    pthread_mutex_unlock(&lock);
  }

  /// must call with turn held
  void threadCreate(pthread_t new_th) {
    assert(self() == activeq.front());
    onThreadCreate(new_th);
    activeq.push_back(new_th);
  }

  pthread_mutex_t *getLock() {
    return &lock;
  }

  RRSchedulerCV() {
    pthread_mutex_init(&lock, NULL);
    for(unsigned i=0; i<MaxThreads; ++i) {
      pthread_cond_init(&waitcv[i], NULL);
      waitvar[i] = NULL;
    }
  }

protected:

  /// same as getTurn() but does not acquire the scheduler lock
  void getTurnNoLock(void) {
    while(self() != activeq.front())
      pthread_cond_wait(&waitcv[self()], &lock);
  }


  /// same as signal() but does not acquire the scheduler lock
  void signalNoLock(void *chan) {
    int tid;
    bool found = false;
    for(std::list<int>::iterator th=waitq.begin(), te=waitq.end();
        th!=te&&!found; ++th) {
      tid = *th;
      if(waitvar[tid] == chan) {
        waitq.erase(th);
        activeq.push_back(tid);
        found = true;
      }
    }
    if(found)
      pthread_cond_signal(&waitcv[tid]);
  }

  std::list<int>       activeq;
  std::list<int>       waitq;
  pthread_mutex_t lock;

  // TODO: can potentially create a thread-local struct for each thread if
  // it improves performance
  pthread_cond_t  waitcv[MaxThreads];
  void*           waitvar[MaxThreads];
};

} // namespace tern

#endif
