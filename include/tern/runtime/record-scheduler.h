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
#include <fstream>
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

  void block() {
    next_op = 0;
  }	//	no block

  void set_op(const char *m_op)
  {
    next_op = m_op;
  }

  static __thread const char *next_op;

  void wakeup() { 
    pthread_mutex_lock(&lock);
    if (next_op)
      ouf << next_op << "_block";
    else
      ouf << "blockingreturn";
    ouf << ' ' << turnCount;
    ouf << ' ' << self();
    ouf << std::endl;

    turnCount++;
    pthread_mutex_unlock(&lock);
  } 
  
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

  ~FCFSScheduler() {
    ouf.close();
  }

  std::ofstream ouf;

  FCFSScheduler(pthread_t main_th): Parent(main_th), ouf("fsfs_message.log") {
    pthread_mutex_init(&lock, NULL);
  }

protected:
  pthread_mutex_t lock; // protects TidMap data
};
#if 0
struct OptScheduler: public Scheduler {
  typedef Scheduler Parent;

  void getTurn(void);
  void putTurn(void);
  void wait(void *chan);
  void block();
  void wakeup();
  void threadCreate(pthread_t new_th);
  void threadBegin(pthread_t self_th);
  void threadEnd(pthread_t self_th);
  void signal(void *chan);
  void broadcast(void *chan);
  void getTurnNU(void);
  void getTurnLN(void);
  void putTurnNU(void);
  void signalNN(void *chan);
  void broadcastNN(void *chan);
  void waitFirstHalf(void *chan, bool doLock = Lock);
  bool isWaiting();
  
  unsigned incTurnCount(void)
  {
    unsigned ret = turnCount++;
    pthread_cond_broadcast(&tickcv);
    return ret;
  }

  pthread_mutex_t *getLock() {
    return &lock;
  }

  RRSchedulerCV(pthread_t main_th);
  ~RRSchedulerCV();

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
  /// select what thread to schedule next, and move it to front of @runq
  virtual void choose(void) {}

  /// for debugging
  void selfcheck(void);
  std::ostream& dump(std::ostream& o);

  std::list<int>  runq;
  std::list<int>  waitq;
  pthread_mutex_t lock;

  struct net_item
  {
    int tid;
    int turn;
  };
  std::list<net_item> net_events;
  pthread_cond_t replaycv;
  pthread_cond_t tickcv;
  FILE *log;
  
  // TODO: can potentially create a thread-local struct for each thread if
  // it improves performance
  pthread_cond_t  waitcv[MaxThreads];
  void*           waitvar[MaxThreads];

};
#endif
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

  void block();

  void wakeup();
  
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
  
  unsigned incTurnCount(void)
  {
    unsigned ret = turnCount++;
    pthread_cond_broadcast(&tickcv);
    return ret;
  }

  pthread_mutex_t *getLock() {
    return &lock;
  }

  RRSchedulerCV(pthread_t main_th);
  ~RRSchedulerCV();

  void check_zombie();

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

  struct net_item
  {
    int tid;
    int turn;
  };
  std::list<net_item> net_events;
  pthread_cond_t replaycv;
  pthread_cond_t tickcv;
  FILE *log;
  pthread_t monitor_th;
  int timemark[MaxThreads];
  int timer;
  
  // TODO: can potentially create a thread-local struct for each thread if
  // it improves performance
  pthread_cond_t  waitcv[MaxThreads];
  void*           waitvar[MaxThreads];
};


/// adapted from an example in POSIX.1-2001
struct Random {
  Random(): next(1) {}
  int rand(int randmax=32767)
  {
    next = next * 1103515245 + 12345;
    return (int)((unsigned)(next/65536) % (randmax + 1));
  }
  void srand(unsigned seed)
  {
    next = seed;
  }
  unsigned long next;
};

/// Instead of round-robin, can schedule threads based on a deterministic
/// (pseudo) random number generator.  That is, at each scheduling
/// decision point, we query the deterministic random number generator for
/// the next thread to run.  Such a scheduler is deterministic, yet it can
/// generate different deterministic sequences based on the seed.
struct SeededRRSchedulerCV: public RRSchedulerCV {
  virtual void choose(void);
  void setSeed(unsigned seed);

  SeededRRSchedulerCV(pthread_t main_th): RRSchedulerCV(main_th) {}

  Random rand;
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
