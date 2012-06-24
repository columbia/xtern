#ifndef __dep_builder_h_
#define __dep_builder_h_

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include "assert.h"

#include <set>
#include <map>

#include "tern/syncfuncs.h"
using namespace std;

struct barrier_logic
{
  barrier_logic(): round(0) {}
  int round;
  int init_op;
  int b_count;
  
  vector<set<int> > first_ops;
  vector<set<int> > second_ops;
  map<int, int> status; 

  //  TODO setup happens-before relation here.

  void init(int count, int idx, int tid, vector<int> &hb)
  {
    init_op = idx;
    b_count = count;
    round = 0;
  }

  void wait(int tid, int idx, vector<int> &hb)
  {
    if (status.find(tid) == status.end()) //  barrier_wait_first
    {
      if ((int) first_ops.size() <= round) first_ops.resize(round + 1);
      if ((int) second_ops.size() <= round) second_ops.resize(round + 1);
      first_ops[round].insert(idx); 
      status[tid] = round;

      if (round == 0)
      {
        //  connect from the init event to the first_wait event in the first round. 
        hb.push_back(init_op); 
      }
    
      if ((int) first_ops[round].size() == b_count)
        ++round;
    } else  // barrier_wait_second
    {
      int r = status[tid];
      status.erase(tid);
      if (second_ops[r].empty()) 
      {
        second_ops[r].insert(idx);
        //  if I'm the first one, connect from every first_wait in this round to me. 
        for (set<int>::iterator it = first_ops[r].begin(); it != first_ops[r].end(); ++it)
          hb.push_back(*it); 
      } else
      {
        //  if I'm not the first one, connect from the first one to me. 
        assert(second_ops[r].size() == 1); 
        hb.push_back(* second_ops[r].begin()); 
      }
    }
  }

  void destroy(int tid, int idx, vector<int> &hb)
  {
    //  this is interesting, maybe some second-op is still on the way.
    //  be careful writing this part!

    //  the naive solution here is to add an edge from any operation 
    //  in the barrier to the destroy operation. 

    //  FIXME it's brute-force
    for (int i = 0; i < round; ++i)
      for (set<int>::iterator it = first_ops[i].begin(); it != first_ops[i].end(); ++it)
        hb.push_back(*it);
  }
};
 
struct mutex_logic
{
  typedef pair<int, int64_t> sig_type;
  map<sig_type, int> unlock_op;
  map<sig_type, int> lock_op;
  map<sig_type, vector<int> > trylock_op;
  map<sig_type, int> status; 
  enum { LOCK, UNLOCK };
#define def_sig sig_type sig = make_pair(pid, mutex);
  void lock(int pid, int tid, int64_t mutex, int idx, vector<int> &hb)
  {
    def_sig;
    assert(status.find(sig) == status.end() || status[sig] == UNLOCK); 
    assert(lock_op.find(sig) == lock_op.end());

    if (unlock_op.find(sig) != unlock_op.end())
    {
      hb.push_back(unlock_op[sig]);
      unlock_op.erase(sig);
    }
    lock_op[sig] = idx;
    status[sig] = LOCK;
    trylock_op[sig].clear();
  }

  void trylock(int pid, int tid, int64_t mutex, int idx, vector<int> &hb)
  {
    def_sig;
    if (status.find(sig) == status.end() || status[sig] == UNLOCK)
    {
      //  successful lock
      lock(pid, tid, mutex, idx, hb); 
    } else
    {
      //  unsuccessful lock
      assert(lock_op.find(sig) != lock_op.end());
      hb.push_back(lock_op[sig]); 
      trylock_op[sig].push_back(idx);
    }
  }

  void unlock(int pid, int tid, int64_t mutex, int idx, vector<int> &hb)
  {
    def_sig;
    assert(status.find(sig) != status.end() || status[sig] == LOCK); 
    assert(unlock_op.find(sig) == unlock_op.end());
    assert(lock_op.find(sig) != lock_op.end());

    for (vector<int>::iterator it = trylock_op[sig].begin(); it != trylock_op[sig].end(); ++it)
      hb.push_back(*it);

    lock_op.erase(sig);
    unlock_op[sig] = idx;
    status[sig] = UNLOCK;
    trylock_op[sig].clear();
  }
#undef def_sig
};

struct cond_logic
{
  typedef pair<int, int64_t> sig_type;
  map<sig_type, int> signal_op;
  map<sig_type, int> broadcast_op;
  typedef pair<int, int> tid_pair; 
  map<sig_type, map<tid_pair, int> > wakeup_set;
  map<sig_type, map<tid_pair, int> > waiting_set;
  map<sig_type, map<tid_pair, int> > failed_set;
  //  FIXME mutex not considered here.
#define def_sig sig_type sig = make_pair(pid, cond);
  void signal(int pid, int tid, int64_t cond, int idx, vector<int> &hb)
  {
    def_sig;
    signal_op[sig] = idx;
  }

  void broadcast(int pid, int tid, int64_t cond, int idx, vector<int> &hb)
  {
    def_sig;
    map<tid_pair, int> &ws = waiting_set[sig];
    map<tid_pair, int> &wake = wakeup_set[sig];
    map<tid_pair, int> &fs = failed_set[sig];

    for (map<tid_pair, int>::iterator it = fs.begin(); it != fs.end(); ++it)
      hb.push_back(it->second);

    for (map<tid_pair, int>::iterator it = ws.begin(); it != ws.end(); ++it)
    {
      hb.push_back(it->second);
      wake[it->first] = idx;
    }
    ws.clear();
  }

  void wait_first(int pid, int tid, int64_t cond, int idx, vector<int> &hb)
  {
    def_sig;
    map<tid_pair, int> &ws = waiting_set[sig];
    ws[make_pair(pid, tid)] = idx;
  }

  void wait_second(int pid, int tid, int64_t cond, int idx, vector<int> &hb)
  {
    def_sig;
    tid_pair tp = make_pair(pid, tid);
    map<tid_pair, int> &wake = wakeup_set[sig];
    map<tid_pair, int> &ws = waiting_set[sig];
    if (wake.find(tp) == wake.end())
    {
      //  wakeup by signal
      assert(signal_op.find(sig) != signal_op.end());
      assert(ws.find(tp) != ws.end());
      hb.push_back(signal_op[sig]);
      signal_op.erase(sig);
      ws.erase(tp);
    } else
    {
      //  wakeup by broadcast
      hb.push_back(wake[tp]);
      wake.erase(tp);
    }
  }

  void wait_failed(int pid, int tid, int64_t cond, int idx, vector<int> &hb)
  {
    def_sig;
    tid_pair tp = make_pair(pid, tid);
    map<tid_pair, int> &fs = failed_set[sig];
    fs[tp] = idx;
  }

#undef def_sig
};

struct sem_logic : public mutex_logic
{
#define def_sig sig_type sig = make_pair(pid, sem);
  void wait(int pid, int tid, int64_t sem, int idx, vector<int> &hb)
  {
    lock(pid, tid, sem, idx, hb);
  }

  void post(int pid, int tid, int64_t sem, int idx, vector<int> &hb)
  {
    def_sig;
    status[sig] = LOCK;
    lock_op[sig] = idx; //  push arbitrary value
    unlock(pid, tid, sem, idx, hb);
  }

  void trywait(int pid, int tid, int64_t sem, int idx, vector<int> &hb)
  {
    trylock(pid, tid, sem, idx, hb);
  }
#undef def_sig
};

#endif
