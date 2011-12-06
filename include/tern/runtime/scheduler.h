/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_SCHED_H
#define __TERN_COMMON_RUNTIME_SCHED_H

#include <assert.h>
#include <tr1/unordered_map>
#include <tr1/unordered_set>

namespace tern {

/// assign an internal tern tid to each pthread tid; also maintains the
/// reverse map from pthread tid to tern tid
struct TidMap {
  enum {MainThreadTid = 0, InvalidTid = -1, MaxThreads = 2000};

  typedef std::tr1::unordered_map<pthread_t, int> pthread_to_tern_map;
  typedef std::tr1::unordered_map<int, pthread_t> tern_to_pthread_map;
  typedef std::tr1::unordered_set<pthread_t>      pthread_tid_set;

  /// create a new tern tid and map pthread_tid to this new id
  int threadCreate(pthread_t pthread_th) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_th);
    assert(it==p_t_map.end() && "pthread tid already in map!");
    p_t_map[pthread_th] = nthread;
    t_p_map[nthread] = pthread_th;
    return nthread++;
  }

  /// sets thread-local tern tid
  void threadBegin(pthread_t self_th) {
    pthread_to_tern_map::iterator it = p_t_map.find(self_th);
    assert(it!=p_t_map.end() && "pthread tid not in map!");
    self_tid = it->second;
  }

  /// remove thread @tern_tid from the maps
  void threadEnd(pthread_t self_th) {
    tern_to_pthread_map::iterator it = t_p_map.find(self());
    assert(it!=t_p_map.end() && "tern tid not in map!");
    assert(self_th==it->second && "mismatch between pthread tid and tern tid!");
    zombies.insert(it->second);
    p_t_map.erase(it->second);
    t_p_map.erase(it);
  }

  void threadJoin(pthread_t pthread_th) {
    zombies.erase(pthread_th);
  }

  int getTernTid(pthread_t pthread_th) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_th);
    if(it!=p_t_map.end())
      return it->second;
    return InvalidTid;
  }

  bool isZombie(pthread_t pthread_th) {
    pthread_tid_set::iterator it = zombies.find(pthread_th);
    return it!=zombies.end();
  }

  /// tern tid for current thread
  static int self() { return self_tid; }
  static __thread int self_tid;

  TidMap(pthread_t main_th) {
    nthread = 0;
    // add tid mappings for main thread
    threadCreate(main_th);
    // initialize self_tid for main thread; main thread will actually call
    // threadBegin() again, but the op is idempotent, so doesn't matter
    threadBegin(main_th);
  }

  pthread_to_tern_map p_t_map;
  tern_to_pthread_map t_p_map;
  pthread_tid_set     zombies;
  int nthread;
};


///  @Scheduler is an empty class mainly for documentation purposes.  It
///  includes a set of methods that'll be implemented by derived scheduler
///  classes.  These methods must follow the following pairing relation:
///
///  methods that gets the turn          methods that gives up the turn
///         @getTurn()                           @putTurn()
///         @beginThread()                       @wait()
///                                              @endThread()
///
///  Any method on the left can pair up with any one on the right.
///
///  Note the Scheduler methods are not virtual by design.  The reason is
///  that pairing up a Scheduler subclass with a Runtime subclass requires
///  rewriting some of the Runtime methods.  That is, we cannot simply
///  plug in a different Scheduler subclass to an existing Runtime
///  subclass.  For this reason, we use the Scheduler subclasses as type
///  arguments to templated Runtime classes, and use method specialization
///  when necessary.  A nice side effect is we get static polymorphism
///  within the Runtime subclasses
struct Scheduler: public TidMap {
  /// get the turn so that other threads trying to get the turn must wait
  void getTurn(void) { }

  /// give up the turn so that other threads can get the turn
  void putTurn(void) { }

  /// wait on @chan and give up turn. @chan can't be NULL.  To avoid @chan
  /// conflicts, should choose @chan values from the same domain.
  void wait(void *chan) { }

  /// wake up a thread waiting on @chan; must call with turn held.  @chan
  /// has the same requirement as wait()
  void signal(void *chan) { }

  /// wake up all threads waiting on @chan; must call with turn held.
  /// @chan has the same requirement as wait
  void broadcast (void *chan) { }

  /// info the scheduler that a thread is calling an external blocking function
  /// NOTICE: different delay before @block() should not lead to different schedule.
  void block() {}

  /// info the scheduler that a blocking thread has returned.
  void wakeup() { incTurnCount(); } 

  /// begin a thread & get the turn; called right after the thread begins
  void threadBegin(pthread_t self_th)  { TidMap:: threadBegin(self_th); }

  /// end a thread & give up turn; called right before the thread exits
  void threadEnd(pthread_t self_th)  { TidMap:: threadEnd(self_th); }

  /// thread @new_th is just created; must call with turn held
  void threadCreate(pthread_t new_th) { TidMap:: threadCreate(new_th); }

  /// join thread @th; must call with turn held
  void threadJoin(pthread_t th) { TidMap:: threadJoin(th); }
  
  /// must call within turn because turnCount is shared across all
  /// threads.  we provide this method instead of incrementing turn for
  /// each successful getTurn() because a successful getTurn() may not
  /// lead to a real success of a synchronization operation (e.g., see
  /// pthread_mutex_lock() implementation)
  unsigned incTurnCount(void) { return turnCount++; }

  Scheduler(pthread_t main_th): TidMap(main_th), turnCount(0) {}

  unsigned turnCount; // number of turns so far
};


}

#endif
