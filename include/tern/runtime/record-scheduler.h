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

extern pthread_mutex_t turn_mutex;

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
    pthread_cond_t cond;
    sem_t    sem;
    void*    chan;
    unsigned timeout;
    int      status; // return value of wait()
    volatile bool wakenUp;

    void reset(int st=0) {
      chan = NULL;
      timeout = FOREVER;
      status = st;
      wakenUp = false;
    }

    wait_t() {
      pthread_cond_init(&cond, NULL);
      sem_init(&sem, 0, 0);
      reset(0);
    }    
    void wait();
    void post();
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
  virtual void next(bool at_thread_end=false, bool hasPoppedFront = false);
  /// child classes can override this method to reorder threads in @runq
  virtual void reorderRunq(void) {}

  /// for debugging
  void selfcheck(void);
  std::ostream& dump(std::ostream& o);

  // MAYBE: can use a thread-local wait struct for each thread if it
  // improves performance
  wait_t waits[MAX_THREAD_NUM];

  //  for monitor
  pthread_t monitor_th;
  int timer;
  int timemark[MAX_THREAD_NUM];

  //  for wakeup
  std::vector<int> wakeup_queue;
  bool wakeup_flag;
  pthread_mutex_t wakeup_mutex;
  void check_wakeup();
  void wakeUpIdleThread();
  void idleThreadCondWait();
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
  virtual void next(bool at_thread_end=false, bool hasPoppedFront = false);
  pthread_mutex_t fcfs_lock;
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
  sem_t waits[MAX_THREAD_NUM];
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
