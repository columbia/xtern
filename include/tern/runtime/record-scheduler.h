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
#include <vector>
#include <map>
#include <errno.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <semaphore.h>
#include "tern/runtime/scheduler.h"

namespace tern {

/// whoever comes first run; nondeterministic
struct RecordSerializer: public Serializer {
  typedef Serializer Parent;

  virtual void getTurn() { pthread_mutex_lock(&lock); }
  virtual void putTurn(bool at_thread_end = false) {
    if(at_thread_end)
      zombify(pthread_self());
    pthread_mutex_unlock(&lock);
  }

  int  wait(void *chan, unsigned timeout = Scheduler::FOREVER) {
    incTurnCount();
    putTurn();
    sched_yield();  //  give control to other threads
    getTurn();
    return 0;
  }

  /// NOTE: This method breaks the Seralizer interface.  Need it to
  /// deterministically record pthread_cond_wait.  See the comments for
  /// pthread_cond_wait in the recorder runtime
  pthread_mutex_t *getLock() {
    return &lock;
  }

  ~RecordSerializer() {
    ouf.close();
  }

  std::ofstream ouf;

  RecordSerializer(): ouf("fsfs_message.log") {
    pthread_mutex_init(&lock, NULL);
  }

protected:
  pthread_mutex_t lock; // protects TidMap data
};


/// TODO: one optimization is to change the single wait queue to be
/// multiple wait queues keyed by the address they wait on, therefore no
/// need to scan the mixed wait queue.
struct RRScheduler: public Scheduler {
  typedef Scheduler Parent;

  struct wait_t {
    sem_t    sem;
    void*    chan;
    unsigned timeout;
    int      status; // return value of wait()

    void reset(int st=0) {
      chan = NULL;
      timeout = FOREVER;
      status = st;
    }

    wait_t() {
      sem_init(&sem, 0, 0);
      reset(0);
    }
  };

  virtual void getTurn();
  virtual void putTurn(bool at_thread_end = false);
  virtual int  wait(void *chan, unsigned timeout = Scheduler::FOREVER);
  virtual void signal(void *chan, bool all=false);

  virtual int block(); 
  virtual void wakeup();

  unsigned incTurnCount(void);
  unsigned getTurnCount(void);

  void childForkReturn();

  RRScheduler();
  ~RRScheduler();

  void detect_blocking_threads();
protected:

  /// timeout threads on @waitq
  int fireTimeouts();
  /// return the next timeout turn number
  unsigned nextTimeout();
  /// pop the @runq and wakes up the thread at the front of @runq
  virtual void next(bool at_thread_end=false);
  /// child classes can override this method to reorder threads in @runq
  virtual void reorderRunq(void) {}

  /// for debugging
  void selfcheck(void);
  std::ostream& dump(std::ostream& o);

  // MAYBE: can use a thread-local wait struct for each thread if it
  // improves performance
  wait_t waits[MaxThreads];

  pthread_mutex_t begin_lock;

  //  for monitor
  pthread_t monitor_th;
  int timer;
  int timemark[MaxThreads];

  //  for wakeup
  std::vector<int> wakeup_queue;
  bool wakeup_flag;
  pthread_mutex_t wakeup_mutex;
  void check_wakeup();
};

struct FCFSScheduler: public RRScheduler {
public:
  virtual void getTurn();
  virtual void putTurn(bool at_thread_end = false);
  virtual int  wait(void *chan, unsigned timeout = Scheduler::FOREVER);
  virtual void signal(void *chan, bool all=false);
  FCFSScheduler();
  ~FCFSScheduler();
protected:
  virtual void next(bool at_thread_end=false);
  pthread_mutex_t fcfs_lock;
};

#if 0
struct RRSchedulerCV: public Scheduler {
  typedef Scheduler Parent;

  enum {Lock=true, NoLock=false, Unlock=true, NoUnlock=false,
        OneThread = false, AllThreads = true };

  void getTurn(bool at_thread_begin)
  { getTurnHelper(Lock, Unlock, at_thread_begin); }
  void putTurn(bool at_thread_end)
  { putTurnHelper(Lock, Unlock, at_thread_end); }

  /// give up the turn and deterministically wait on @chan as the last
  /// thread on @waitq
  void wait(void *chan) { waitHelper(chan, Lock, Unlock); }

  void block();

  void wakeup();
/*
  removed in sem branch
  /// must call with turn held
  void threadCreate(pthread_t new_th) {
    assert(self() == runq.front());
    Parent::threadCreate(new_th);
    runq.push_back(getTernTid(new_th));
    timemark[getTernTid(new_th)] = -1;
  }
*/

#if 0
  void threadBegin(pthread_t self_th) {
    pthread_mutex_lock(&lock);
    Parent::threadBegin(self_th);
    getTurnHelper(false, false);
    pthread_mutex_unlock(&lock);
  }

  /// if any thread is waiting on our exit, wake it up, then give up turn
  /// and exit (not putting self back to the tail of runq)
  void threadEnd(pthread_t self_th);
#endif

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

  void detect_blocking_threads();

protected:

  /// same as getTurn but acquires or releases the scheduler lock based on
  /// the flags @doLock and @doUnlock
  void getTurnHelper(bool doLock, bool doUnlock, bool at_thread_begin=false);
  /// same as putTurn but acquires or releases the scheduler lock based on
  /// the flags @doLock and @doUnlock
  void putTurnHelper(bool doLock, bool doUnlock, bool at_thread_end=false);
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
#endif

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
struct SeededRRScheduler: public RRScheduler {
  virtual void reorderRunq(void);
  void setSeed(unsigned seed);
  Random rand;
};


/// replay scheduler using semaphores
struct ReplaySchedulerSem: public RecordSerializer {
public:
  ReplaySchedulerSem();
  ~ReplaySchedulerSem();

  /*  inherent from parent  */
  virtual void getTurn();
  virtual void putTurn(bool at_thread_end = false);
  //int  wait(void *chan, unsigned timeout = Scheduler::FOREVER);
  //void signal(void *chan, bool all=false);
  //virtual int block(); 
  //void wakeup();
  //unsigned incTurnCount(void);
  //unsigned getTurnCount(void);
  //void childForkReturn();

  typedef std::map<std::string, std::string> record_type;
  struct record_list : public std::vector<record_type>
  {
    record_list()
      : std::vector<record_type>(), pos(0) {}
    record_type &next() { return (*this)[pos]; }
    bool has_next() { return pos < (int) this->size(); }
    void move_next() { ++pos; }
    int pos;
  };

protected:
  sem_t waits[MaxThreads];
  std::vector<record_list> logdata;
  void readrecords(FILE * fin, record_list &records);

  bool wakeup_flag;
  void check_wakeup() {}

};

/// replay scheduler using integer flags
struct ReplaySchedulerFlag: public Scheduler {
  // TODO
};

} // namespace tern

#endif
