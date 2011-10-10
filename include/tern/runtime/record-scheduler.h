/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code
//
// TODO:
// 1. use multiple wait queues per waitvar, instead of one for all
//    waitvars, for performance?
// 2. implement random scheduler
// 3. implement replay scheduler
// 4. support break out of turn.  RR can deadlock if program uses ad hoc
//    sync, such as "while(flag)"

#ifndef __TERN_RECORDER_SCHEDULER_H
#define __TERN_RECORDER_SCHEDULER_H

#include <list>
#include <errno.h>
#include <pthread.h>
#include <iostream>
#include <semaphore.h>
#include "tern/runtime/scheduler.h"

namespace tern {

/// whoever comes first run; nondeterministic
struct FCFSScheduler: public Scheduler {
  typedef Scheduler Parent;

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

  // no signal() or broadcast for FCFS

  void threadBegin(pthread_t self_th) {
    getTurn();
    Parent::threadBegin(self_th);
  }

  void threadEnd(pthread_t self_th) {
    Parent::threadEnd(self_th);
    putTurn();
  }

  pthread_mutex_t *getLock() {
    return &lock;
  }

  FCFSScheduler(pthread_t main_th): Parent(main_th) {
    pthread_mutex_init(&lock, NULL);
  }

protected:
  pthread_mutex_t lock; // protects TidMap data
};


/// TODO: one optimization is to change the single wait queue to be
/// multiple wait queues keyed by the address they wait on, therefore no
/// need to scan the mixed wait queue.
struct RRSchedulerCV: public Scheduler {
  typedef Scheduler Parent;

  enum {Lock=true, NoLock=false, Unlock=true, NoUnlock=false,
        OneThread = false, AllThreads = true };

  void getTurn(void)    { getTurnHelper(Lock, Unlock); }
  void putTurn(void)    { putTurnHelper(Lock, Unlock); }

  /// give up the turn and deterministically wait on @chan as the last
  /// thread on @waitq
  void wait(void *chan) { waitHelper(chan, Lock, Unlock); }

  /// must call with turn held
  void threadCreate(pthread_t new_th) {
    assert(self() == runq.front());
    Parent::threadCreate(new_th);
    runq.push_back(getTernTid(new_th));
  }

  void threadBegin(pthread_t self_th) {
    pthread_mutex_lock(&lock);
    Parent::threadBegin(self_th);
    getTurnHelper(false, false);
    pthread_mutex_unlock(&lock);
  }

  /// if any thread is waiting on our exit, wake it up, then give up turn
  /// and exit (not putting self back to the tail of runq)
  void threadEnd(pthread_t self_th);

  /// deterministically wake up the first thread waiting on @chan on the
  /// wait queue; must call with the turn held
  ///
  /// pthread_mutex_lock() is necessary to avoid race with the
  /// pthread_cond_wait() in void wait(void* chan) illustrated by the
  /// thread interleaving below
  ///
  ///                    this->wait()
  ///                      blocks within pthread_cond_wait
  ///   this->getTurn()
  ///     unlocks lock
  ///   this->signal()     wakes up from pthread_cond_wait()
  ///
  void signal(void *chan)     { signalHelper(chan, OneThread, Lock, Unlock); }

  /// deterministically wake up all threads waiting on @chan on the wait
  /// queue; must call with the turn held.
  ///
  ///  need to acquire @lock for the same reason as in signal()
  void broadcast(void *chan)  { signalHelper(chan, true, Lock, Unlock); }

  /// for implementing pthread_cond_wait
  void getTurnNU(void)        { getTurnHelper(NoLock, Unlock); }
  void getTurnLN(void)        { getTurnHelper(Lock, NoUnlock); }
  void putTurnNU(void)        { putTurnHelper(NoLock, Unlock); }
  void signalNN(void *chan)   { signalHelper(chan, OneThread,
                                             NoLock, NoUnlock); }
  void broadcastNN(void *chan){ signalHelper(chan, AllThreads,
                                             NoLock, NoUnlock); }
  void waitFirstHalf(void *chan, bool doLock = Lock);
  bool isWaiting();

  pthread_mutex_t *getLock() {
    return &lock;
  }

  RRSchedulerCV(pthread_t main_th);

protected:

  /// same as getTurn but acquires or releases the scheduler lock based on
  /// the flags @doLock and @doUnlock
  void getTurnHelper(bool doLock, bool doUnlock);
  /// same as putTurn but acquires or releases the scheduler lock based on
  /// the flags @doLock and @doUnlock
  void putTurnHelper(bool doLock, bool doUnlock);
  /// same as signal() but acquires or releases the scheduler lock based
  /// on the flags @doLock and @doUnlock
  void signalHelper(void *chan, bool all, bool doLock,
                    bool doUnlock, bool wild = false);
  /// same as wait but but acquires or releases the scheduler lock based
  /// on the flags @doLock and @doUnlock
  void waitHelper(void *chan, bool doLock, bool doUnlock);
  /// common operations done by both wait() and putTurnHelper()
  void next(void);

  /// for debugging
  void selfcheck(void);
  std::ostream& dump(std::ostream& o);

  std::list<int>  runq;
  std::list<int>  waitq;
  pthread_mutex_t lock;

  // TODO: can potentially create a thread-local struct for each thread if
  // it improves performance
  pthread_cond_t  waitcv[MaxThreads];
  void*           waitvar[MaxThreads];
};


/// Instead of round-robin, can easy use a deterministic but random
/// sequence using a deterministic pseodu random number generator.  at
/// each decision point, toss a coin to decide which one to run.
struct RandomScheduler: public Scheduler {
  // TODO
};

/// replay scheduler using semaphores
struct ReplaySchedulerSem: public Scheduler {
  // TODO
};

/// replay scheduler using integer flags
struct ReplaySchedulerFlag: public Scheduler {
  // TODO
};

} // namespace tern

#endif
