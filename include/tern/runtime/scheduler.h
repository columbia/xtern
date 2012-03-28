/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_SCHED_H
#define __TERN_COMMON_RUNTIME_SCHED_H

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <list>
#include <tr1/unordered_map>
#include <tr1/unordered_set>

namespace tern {

/// assign an internal tern tid to each pthread tid; also maintains the
/// reverse map from pthread tid to tern tid.  This class itself doesn't
/// synchronize its methods; instead, the callers of these methods must
/// ensure that the methods are synchronized.
struct TidMap {
  enum {MainThreadTid = 0, InvalidTid = -1, MaxThreads = 2000};

  typedef std::tr1::unordered_map<pthread_t, int> pthread_to_tern_map;
  typedef std::tr1::unordered_map<int, pthread_t> tern_to_pthread_map;
  typedef std::tr1::unordered_set<pthread_t>      pthread_tid_set;

  /// create a new tern tid and map pthread_tid to this new id
  int create(pthread_t pthread_th) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_th);
    assert(it==p_t_map.end() && "pthread tid already in map!");
    p_t_map[pthread_th] = nthread;
    t_p_map[nthread] = pthread_th;
    return nthread++;
  }

  /// sets thread-local tern tid to be the tid of @self_th
  void self(pthread_t self_th) {
    pthread_to_tern_map::iterator it = p_t_map.find(self_th);
    if (it==p_t_map.end())
      fprintf(stderr, "pthread tid not in map!\n");
    assert(it!=p_t_map.end() && "pthread tid not in map!");
    self_tid = it->second;
  }

  /// remove thread @tern_tid from the maps and insert it into the zombie set
  void zombify(pthread_t self_th) {
    tern_to_pthread_map::iterator it = t_p_map.find(self());
    assert(it!=t_p_map.end() && "tern tid not in map!");
    assert(self_th==it->second && "mismatch between pthread tid and tern tid!");
    zombies.insert(it->second);
    p_t_map.erase(it->second);
    t_p_map.erase(it);
  }

  /// remove thread @pthread_th from the maps
  void reap(pthread_t pthread_th) {
    zombies.erase(pthread_th);
  }

  /// return tern tid of thread @pthread_th
  int getTid(pthread_t pthread_th) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_th);
    if(it!=p_t_map.end())
      return it->second;
    return InvalidTid;
  }

  /// return if thread @pthread_th is in the zombie set
  bool zombie(pthread_t pthread_th) {
    pthread_tid_set::iterator it = zombies.find(pthread_th);
    return it!=zombies.end();
  }

  /// tern tid for current thread
  static int self() { return self_tid; }
  static __thread int self_tid;

  TidMap(pthread_t main_th) { init(main_th); }

  /// initialize state
  void init(pthread_t main_th) {
    nthread = 0;
    // add tid mappings for main thread
    create(main_th);
    // initialize self_tid for main thread in case the derived class
    // constructors call self().  The main thread may call
    // @self(pthread_self()) again to set @self_tid, but this assignment
    // is idempotent, so it doesn't matter
    self(main_th);
  }

protected:

  /// reset internal state to initial state
  void reset(pthread_t main_th) {
    p_t_map.clear();
    t_p_map.clear();
    zombies.clear();

    init(main_th);
  }

  pthread_to_tern_map p_t_map;
  tern_to_pthread_map t_p_map;
  pthread_tid_set     zombies;
  int nthread;
};


/// @Serializer defines the interface for a serializer that ensures that
/// all synchronizations are serialized.  A serializer doesn't attempt to
/// schedule them.  The nondeterministic recorder runtime and the replay
/// runtime use serializers, instead of schedulers.
struct Serializer: public TidMap {
  /// get the turn so that other threads trying to get the turn must wait
  void getTurn() { }

  /// give up the turn so that other threads can get the turn.  this
  /// method should also increment turnCount
  void putTurn(bool at_thread_end=false) { }

  /// inform the serializer that a thread is calling an external blocking
  /// function.
  ///
  /// NOTICE: different delay before @block() should not lead to different
  /// schedule.
  void block() {}

  /// inform the scheduler that a blocking thread has returned.
  void wakeup() {}

  /// inform the serializer that thread @new_th is just created; must call
  /// with turn held
  void create(pthread_t new_th) { TidMap::create(new_th); }

  /// inform the serializer that thread @th just joined; must call with
  /// turn held
  void join(pthread_t th) { TidMap::reap(th); }

  /// child process begins
  void childForkReturn() { TidMap::reset(pthread_self()); }

  /// NOTE: RecordSerializer needs this method to implement
  /// pthread_cond_*wait
  pthread_mutex_t *getLock() { return NULL; }

  /// must call within turn because turnCount is shared across all
  /// threads.  we provide this method instead of incrementing turn for
  /// each successful getTurn() because a successful getTurn() may not
  /// lead to a real success of a synchronization operation (e.g., see
  /// pthread_mutex_lock() implementation)
  unsigned incTurnCount(void) { return turnCount++; }
  unsigned getTurnCount(void) { return turnCount;   }

  Serializer(): TidMap(pthread_self()), turnCount(0) {}

  unsigned turnCount; // number of turns so far
};


///  @Scheduler defines the interface for a deterministic scheduler that
///  ensures that all synchronizations are deterministically scheduled.
///  The DMT runtime uses DMT schedulers.
///
///  The @wait() method method gives up the turn and then re-grabs it upon
///  return.  They are similar to pthread_cond_wait() and
///  pthread_cond_timedwait() which gives up the corresponding lock then
///  re-grabs it.
///
///  Note the Scheduler methods are not virtual by design.  The reason is
///  that pairing up a Scheduler subclass with a Runtime subclass requires
///  rewriting some of the Runtime methods.  That is, we cannot simply
///  plug in a different Scheduler subclass to an existing Runtime
///  subclass.  For this reason, we use the Scheduler subclasses as type
///  arguments to templated Runtime classes, and use method specialization
///  when necessary.  A nice side effect is we get static polymorphism
///  within the Runtime subclasses
struct Scheduler: public Serializer {

  enum {FOREVER = UINT_MAX}; // wait forever w/o timeout

  /// wait on @chan until another thread calls signal(@chan), or turnCount
  /// is greater than or equal to @timeout if @timeout is not 0.  give up
  /// turn and re-grab it upon return.
  ///
  /// To avoid @chan conflicts, should choose @chan values from the same
  /// domain.  @chan can be NULL; @wait(NULL, @timeout) simply means sleep
  /// until @timeout
  ///
  /// @return 0 if wait() is signaled or ETIMEOUT if wait() times out
  int wait(void *chan, unsigned timeout=FOREVER) { return 0; }

  /// wake up one thread (@all = false) or all threads (@all = true)
  /// waiting on @chan; must call with turn held.  @chan has the same
  /// requirement as wait()
  void signal(void *chan, bool all = false) { }

  void create(pthread_t new_th) {
    assert(self() == runq.front());
    TidMap::create(new_th);
    runq.push_back(getTid(new_th));
  }

  void childForkReturn() {
    TidMap::reset(pthread_self());
    waitq.clear();
    runq.clear();
    runq.push_back(MainThreadTid);
  }

  std::list<int>  runq;
  std::list<int>  waitq;
};


}

#endif
