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
  int onThreadCreate(pthread_t pthread_tid) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_tid);
    assert(it==p_t_map.end() && "pthread tid already in map!");
    p_t_map[pthread_tid] = nthread;
    t_p_map[nthread] = pthread_tid;
    return nthread++;
  }

  /// sets thread-local tern tid
  void onThreadBegin() {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_self());
    assert(it!=p_t_map.end() && "pthread tid not in map!");
    self_tid = it->second;
  }

  /// remove thread @tern_tid from the maps
  void onThreadEnd() {
    tern_to_pthread_map::iterator it = t_p_map.find(self());
    assert(it!=t_p_map.end() && "tern tid not in map!");
    zombies.insert(it->second);
    p_t_map.erase(it->second);
    t_p_map.erase(it);
  }

  void onThreadJoin(pthread_t pthread_tid) {
    zombies.erase(pthread_tid);
  }

  int getTernTid(pthread_t pthread_tid) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_tid);
    if(it!=p_t_map.end())
      return it->second;
    return InvalidTid;
  }

  bool isZombie(pthread_t pthread_tid) {
    pthread_tid_set::iterator it = zombies.find(pthread_tid);
    return it!=zombies.end();
  }

  /// tern tid for current thread
  static int self() { return self_tid; }
  static __thread int self_tid;

  TidMap() {
    nthread = 0;
    onThreadCreate(pthread_self()); // main thread
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

  /// begin a thread & get the turn; called right after the thread begins
  void threadBegin(void)  { }

  /// end a thread & give up turn; called right before the thread exits
  void threadEnd(void)  { }

  /// thread @new_th is just created; must call with turn held
  void threadCreate(pthread_t new_th) { }
};


}

#endif
