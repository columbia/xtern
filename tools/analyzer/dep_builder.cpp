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
#include "analyzer.h"
#include "dep_builder.h"

using namespace std;
using namespace tern;

extern bool compute_signature;
extern bool po_signature;
extern bool bdb_spec;
extern uint64_t turn_limit;

void build_barrier_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow)
{
  printf("start building barrier partial order\n\n");
  int n = ops.size();

 
  typedef pair<int, string> barrier_sig_t;
  map<barrier_sig_t, barrier_logic > barrier_status;

  for (int i = 0; i < n;++i)
  {
    switch (ops[i].rec.op)
    {
    case syncfunc::pthread_barrier_init:
    {
      assert(ops[i].rec.args.size() > 1 && "missing arguments");
      barrier_sig_t barrier = make_pair(ops[i].pid, ops[i].get_string(0)); 
      assert(barrier_status.find(barrier) == barrier_status.end()
        && "trying to init an existing barrier!");
      barrier_status[barrier].init(ops[i].get_int(1), ops[i].tid, i, hb_arrow[i]);
      break;
    }
    case syncfunc::pthread_barrier_wait:
    {
      assert(ops[i].rec.args.size() > 0 && "missing arguments");
      barrier_sig_t barrier = make_pair(ops[i].pid, ops[i].get_string(0)); 
      assert(barrier_status.find(barrier) != barrier_status.end()
        && "trying to wait for a non-existing barrier!");
      barrier_status[barrier].wait(ops[i].tid, i, hb_arrow[i]);
      break;
    }
    case syncfunc::pthread_barrier_destroy:
    {
      assert(ops[i].rec.args.size() > 0 && "missing arguments");
      barrier_sig_t barrier = make_pair(ops[i].pid, ops[i].get_string(0)); 
      assert(barrier_status.find(barrier) != barrier_status.end()
        && "trying to destroy a non-existing barrier!");
      barrier_status[barrier].destroy(ops[i].tid, i, hb_arrow[i]);
      break;
    }

    default:
      // skip it, not my work
      break;
    }
  }
}

void build_create_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow)
{
  printf("start building create partial order\n\n");
  int n = ops.size();

  typedef pair<int, int64_t> tid_pair;
  map<tid_pair, int> create_op;
  map<tid_pair, int> end_op;
  set<tid_pair> thread_pool;

  set<int> main_thread;

  for (int i = 0; i < n;++i)
  {
    switch (ops[i].rec.op)
    {
    case syncfunc::pthread_create:
    {
      tid_pair child = make_pair(ops[i].pid, ops[i].get_int64(0));
//      cerr << "create (" << child.first << ", " << child.second << ")" << endl;
      assert(create_op.find(child)  == create_op.end());
      create_op[child] = i;
      break;
    }
    case syncfunc::pthread_join:
    {
      tid_pair child = make_pair(ops[i].pid, ops[i].get_int64(0));
//      cerr << "join (" << child.first << ", " << child.second << ")" << endl;
      assert(end_op.find(child)  != end_op.end());
      hb_arrow[i].push_back(end_op[child]);
      end_op.erase(child);
      break;
    }
    case syncfunc::tern_thread_end:
    {
      tid_pair me = make_pair(ops[i].pid, ops[i].get_int64(0));
//      cerr << "end (" << me.first << ", " << me.second << ")" << endl;
      assert(end_op.find(me) == end_op.end());
      end_op[me] = i;
      break;
    }
    case syncfunc::tern_thread_begin:
    {
      tid_pair me = make_pair(ops[i].pid, ops[i].get_int64(0));
//      cerr << "begin (" << me.first << ", " << me.second << ")" << endl;
      if (main_thread.find(ops[i].pid) != main_thread.end())
      {
        //assert(create_op.find(me) != create_op.end());
        hb_arrow[i].push_back(create_op[me]);
        create_op.erase(me);
      } else
        main_thread.insert(ops[i].pid);
      break;
    }
    default:
      // skip it, not my work
      break;
    }
  }
}

void build_mutex_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow)
{
  printf("start building mutex partial order\n\n");
  int n = ops.size();
  cond_logic cl;
  mutex_logic ml;
  typedef pair<int, int> tid_pair;
  set<tid_pair> cond_wait_flag;
  for (int i = 0; i < n;++i)
  {
    tid_pair tp = make_pair(ops[i].pid, ops[i].tid);
    switch (ops[i].rec.op)
    {
    case syncfunc::pthread_mutex_lock:
    {
      ml.lock(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    case syncfunc::pthread_mutex_unlock:
    {
      ml.unlock(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    case syncfunc::pthread_mutex_trylock:
    {
      ml.trylock(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    case syncfunc::pthread_cond_signal:
    {
      //  ops[i].get_int64(1) is mutex
      cl.signal(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    case syncfunc::pthread_cond_broadcast:
    {
      //  ops[i].get_int64(1) is mutex
      cl.broadcast(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    case syncfunc::pthread_cond_wait:
    {
      if (cond_wait_flag.find(tp) == cond_wait_flag.end())
      {
        //  cond_wait_first
        cond_wait_flag.insert(tp);
        
        //  cond_wait_first is treated as a mutex_unlock
        ml.unlock(ops[i].pid, ops[i].tid, ops[i].get_int64(1), i, hb_arrow[i]);
        cl.wait_first(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      } else
      {
        //  cond_wait_second
        cond_wait_flag.erase(tp);

        //  cond_wait_first is treated as a mutex_lock and a cond_wait
        ml.lock(ops[i].pid, ops[i].tid, ops[i].get_int64(1), i, hb_arrow[i]);
        cl.wait_second(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      }
      break;
    }
    case syncfunc::pthread_cond_timedwait:
    {
      if (cond_wait_flag.find(tp) == cond_wait_flag.end())
      {
        //  cond_timedwait_first
        cond_wait_flag.insert(tp);
        
        //  cond_wait_first is treated as a mutex_unlock
        ml.unlock(ops[i].pid, ops[i].tid, ops[i].get_int64(1), i, hb_arrow[i]);
        cl.wait_first(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      } else
      {
        //  cond_timedwait_second
        cond_wait_flag.erase(tp);

        //  cond_wait_first is treated as a mutex_lock and a cond_wait
        ml.lock(ops[i].pid, ops[i].tid, ops[i].get_int64(1), i, hb_arrow[i]);
        int ret = ops[i].get_int(2);
        if (!ret) //  success
          cl.wait_second(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
        else
          cl.wait_failed(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      }
      break;
    }
    default:
      //  skip it, not my work
      break;
    }
  }
}

void build_sem_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow)
{
  printf("start building semaphore partial order\n\n");
  int n = ops.size();
  sem_logic sl;
  for (int i = 0; i < n;++i)
  {
    switch (ops[i].rec.op)
    {
    case syncfunc::sem_wait:
    {
      //  ops[i].get_int64(1) is mutex
      sl.wait(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    case syncfunc::sem_timedwait:
    {
      assert(false && "sem_timewait not handled yet.");
      break;
    }
    case syncfunc::sem_post:
    {
      sl.post(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    case syncfunc::sem_trywait:
    {
      sl.trywait(ops[i].pid, ops[i].tid, ops[i].get_int64(0), i, hb_arrow[i]);
      break;
    }
    default:
      //  skip it, not my job.
      break;
    }
  }
}


