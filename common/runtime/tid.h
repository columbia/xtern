/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_TID_H
#define __TERN_COMMON_RUNTIME_TID_H

#include <assert.h>
#include <tr1/unordered_map>
#include <tr1/unordered_set>

namespace tern {

struct tid_manager {
  enum {InvalidTid = -1, MaxThreads = 100};

  typedef std::tr1::unordered_map<pthread_t, int> pthread_to_tern_map;
  typedef std::tr1::unordered_map<int, pthread_t> tern_to_pthread_map;
  typedef std::tr1::unordered_set<pthread_t>      pthread_tid_set;

  tid_manager(): nthread(0) { }

  /// create a new tern tid and map pthread_tid to this new id
  int on_thread_create(pthread_t pthread_tid) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_tid);
    assert(it==p_t_map.end() && "pthread tid already in map!");
    p_t_map[pthread_tid] = nthread;
    t_p_map[nthread] = pthread_tid;
    return nthread++;
  }

  /// sets thread local tid
  void on_thread_begin(pthread_t pthread_tid) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_tid);
    assert(it!=p_t_map.end() && "pthread tid not in map!");
    self_tid = it->second;
  }

  /// remove tid from the maps, and mark it exited
  int on_thread_end(void) {
    int tid = self();
    tern_to_pthread_map::iterator it = t_p_map.find(tid);
    assert(it!=t_p_map.end() && "tern tid not in map!");
    assert(exited_threads.find(it->second)==exited_threads.end());
    exited_threads.insert(it->second);
    p_t_map.erase(it->second);
    t_p_map.erase(it);
    return tid;
  }

  bool alive(pthread_t pthread_tid) {
    return p_t_map.find(pthread_tid) != p_t_map.end();
  }

  bool exited(pthread_t pthread_tid) {
    return exited_threads.find(pthread_tid) != exited_threads.end();
  }

  pthread_t get_pthread_tid(int tern_tid) {
    tern_to_pthread_map::iterator it = t_p_map.find(tern_tid);
    if(it!=t_p_map.end())
      return it->second;
    return InvalidTid;
  }

  int get_tern_tid(pthread_t pthread_tid) {
    pthread_to_tern_map::iterator it = p_t_map.find(pthread_tid);
    if(it!=p_t_map.end())
      return it->second;
    return InvalidTid;
  }

  bool is_main_thread() {
    return nthread == 0;
  }

  int num_thread(void) {
    return nthread;
  }

  pthread_to_tern_map p_t_map;
  tern_to_pthread_map t_p_map;
  // we need to track exited threads because a buggy user program may wait
  // on pthread_t that's never created
  pthread_tid_set exited_threads;
  int nthread;

  /// tern tid for current thread
  static int self() { return self_tid; }
  static __thread int self_tid;
};

extern tid_manager tids;

}

#endif
