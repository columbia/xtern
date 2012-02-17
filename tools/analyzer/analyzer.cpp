#include "log_reader.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string>
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
using namespace tern;

struct op_t 
{
  record_t rec;
  int pid;
  int tid;
  string get_string(int idx)
  {
    if (idx >= 0 && idx < (int) rec.args.size())
      return rec.args[idx];
    else
      return "";
  }
#define cur_rec rec
  int get_int(int idx)
  {
    if (idx >= 0 && idx < (int) cur_rec.args.size())
    {
      unsigned int x;
      sscanf(cur_rec.args[idx].c_str(), "%x", &x);
      return x; 
    }
    else
      return -1;
  }

  int64_t get_int64(int idx)
  {
    if (idx >= 0 && idx < (int) cur_rec.args.size())
    {
      uint64_t x;
      sscanf(cur_rec.args[idx].c_str(), "%lx", &x);
      return x; 
    }
    else
      return -1;
  }
#undef cur_rec
};

int64_t get_time(string &st)
{
  size_t p = st.find_first_of(':'); 
  if (p == string::npos) return atoi(st.c_str());
  int64_t sec = atoi(st.substr(0, p - 1).c_str());
  int64_t nsec = atoi(st.substr(p + 1).c_str());
  return sec * 1000000000 + nsec;
}

int getdir (string dir, vector<string> &files)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
        files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}

bool compare_by_turn( const op_t &x, const op_t &y)
{
  return x.rec.turn < y.rec.turn;
}

void usage(int argc,  char *argv[])
{
  fprintf(stderr, "Usage\n\t%s <log directory>\n", argv[0]);
  exit(1);
}

vector<op_t> ops;
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
      assert(create_op.find(child)  == create_op.end());
      create_op[child] = i;
      break;
    }
    case syncfunc::pthread_join:
    {
      tid_pair child = make_pair(ops[i].pid, ops[i].get_int64(0));
      assert(end_op.find(child)  != end_op.end());
      hb_arrow[i].push_back(end_op[child]);
      end_op.erase(child);
      break;
    }
    case syncfunc::tern_thread_end:
    {
      tid_pair me = make_pair(ops[i].pid, ops[i].get_int64(0));
      assert(end_op.find(me) == end_op.end());
      end_op[me] = i;
      break;
    }
    case syncfunc::tern_thread_begin:
    {
      tid_pair me = make_pair(ops[i].pid, ops[i].get_int64(0));
      if (main_thread.find(ops[i].pid) != main_thread.end())
      {
        assert(create_op.find(me) != create_op.end());
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

#define OUF stderr
#include "printer.xx"
#undef OUF
  struct timerec
  {
    unsigned op;
    int count;
    int64_t sum_apt;
    double sum_sqr_apt;
    int64_t sum_syt;
    double sum_sqr_syt;
    int64_t sum_sct;
    double sum_sqr_sct;
  };

void analyze_time(vector<op_t> &ops)
{
  map<unsigned, timerec> res; 
  map<unsigned, vector<int64_t> > apt_collection; 
  map<unsigned, vector<int64_t> > syt_collection; 
  map<unsigned, vector<int64_t> > sct_collection; 
  for (size_t i = 0; i < ops.size(); ++i)
  {
    op_t &op = ops[i];
    unsigned insid = op.rec.insid; 
    int64_t apt = get_time(op.rec.app_time);
    int64_t syt = get_time(op.rec.syscall_time);
    int64_t sct = get_time(op.rec.sched_time);
    if (res.find(insid) == res.end())
    {
      timerec &rec = res[insid];
      rec.count = 0;
      rec.sum_apt = rec.sum_syt = rec.sum_sct = 0;
      rec.sum_sqr_apt = rec.sum_sqr_syt = rec.sum_sqr_sct = 0;
    }
    timerec &rec = res[insid];
    vector<int64_t> &aptcol = apt_collection[insid];
    vector<int64_t> &sytcol = syt_collection[insid];
    vector<int64_t> &sctcol = sct_collection[insid];
    rec.count++;
    rec.op = op.rec.op;
    rec.sum_apt += apt;
    rec.sum_syt += syt;
    rec.sum_sct += sct;
    rec.sum_sqr_apt += apt * apt;
    rec.sum_sqr_syt += syt * syt;
    rec.sum_sqr_sct += sct * sct;
    aptcol.push_back(apt); 
    sytcol.push_back(syt); 
    sctcol.push_back(sct); 
  }
  FILE *ouf = fopen("detail.log", "w");
  for (map<unsigned, timerec>::iterator it = res.begin(); it != res.end(); ++it)
  {
    fprintf(ouf, "%08x apt, ", it->first);
    fprintf(ouf, "%08x syt, ", it->first);
    fprintf(ouf, "%08x sct, ", it->first);
  }
  fprintf(ouf, "\n");
  while(true)
  {
    int count = 0;
    for (map<unsigned, timerec>::iterator it = res.begin(); it != res.end(); ++it)
    {
      unsigned insid = it->first;
      vector<int64_t> &aptcol = apt_collection[insid];
      vector<int64_t> &sytcol = syt_collection[insid];
      vector<int64_t> &sctcol = sct_collection[insid];
      count += aptcol.size() || sytcol.size() || sctcol.size();
      if (aptcol.size()) { fprintf(ouf, "%lld", aptcol.back()); aptcol.pop_back(); } 
      fprintf(ouf, ", "); 
      if (sytcol.size()) { fprintf(ouf, "%lld", sytcol.back()); sytcol.pop_back(); } 
      fprintf(ouf, ", "); 
      if (sctcol.size()) { fprintf(ouf, "%lld", sctcol.back()); sctcol.pop_back(); } 
      fprintf(ouf, ", "); 
    }
    fprintf(ouf, "\n");
    if (!count) break;
  }
  fclose(ouf);
  printf("eip        "
         "count    "
         "app time  "
         "app time std     "
         "sysc time "
         "sysc time std    "
         "schd time "
         "schd time std    "
         "\n");

  for (map<unsigned, timerec>::iterator it = res.begin(); it != res.end(); ++it)
  {
    timerec &rec = it->second;
    printf("0x%08x", it->first); 

    double avg;
    printf(" %08d", rec.count);
    //printf(" %lld", rec.sum_sct);
#define defprint(x) \
    printf(" %09lld", rec.sum_##x / rec.count); \
    avg = rec.sum_##x * double(1.0) / rec.count;\
    printf(" %016.04f", sqrt(rec.sum_sqr_##x - 2 * avg * rec.sum_##x + rec.count * avg * avg));
    defprint(apt);
    defprint(syt);
    defprint(sct);
#undef defprint
    printf(" %s\n", syncfunc::getName(rec.op));
  }
}

void analyze(vector<op_t> &ops)
{
  printf("\n\nstart analyzing execution trace ... \n\n");

  sort(ops.begin(), ops.end(), compare_by_turn);
  int n = (int) ops.size();
  map<pair<int, int>, int> last_op;
  vector<vector<int> > hb_arrow;
  hb_arrow.resize(n);

  for (int i = 0; i < n; ++i)
  {
    pair<int, int> p = make_pair(ops[i].tid, ops[i].tid);
    if (last_op.find(p) != last_op.end())
      hb_arrow[i].push_back(last_op[p]);
    last_op[p] = i;
  }

  build_barrier_hb(ops, hb_arrow);
  build_create_hb(ops, hb_arrow);
  build_mutex_hb(ops, hb_arrow);
  build_sem_hb(ops, hb_arrow);

  print_trace(ops, hb_arrow);
}

int main(int argc, char *argv[])
{
  if (argc < 2)
    usage(argc, argv);

  const char *filename = argv[1];
  printf("Reading log files from directory %s\n", filename);

  vector<string> files; 
  if (!getdir(string(filename), files))
  {
    set<int> pid_set;
    map<int, set<int> > tid_set;
    int log_file_count = 0;

    for (int i = 0; i < (int)files.size(); ++i)
    {
      //cout << files[i] << endl;
      printf("processing %s\n", files[i].c_str());
      int pid = -1, tid = -1;
      sscanf(files[i].c_str(), "tid-%d-%d.txt", &pid, &tid);
      if (pid < 0 || tid < 0)
      {
        printf("skip file %s\n", files[i].c_str());
        continue;
      }

      pid_set.insert(pid);
      tid_set[pid].insert(tid);
      ++log_file_count;

      printf("reading from log file %s\n", files[i].c_str());
      log_reader *reader = new txt_log_reader();
      string ab_path = string(filename) + "/" + files[i];
      if (!reader->open(ab_path.c_str()))
      {
        fprintf(stderr, "cannot open file\n");
        continue;
      }

      while (reader->valid())
      {
        ops.resize(ops.size() + 1);
        ops.back().pid = pid;
        ops.back().tid = tid;
        ops.back().rec = reader->get_current_rec();
        reader->next();
      }

      delete reader;
    }

    printf("collected %d records from %d processes and %d log files.\n", 
      (int) ops.size(), (int) pid_set.size(), log_file_count);
    for (map<int, set<int> >::iterator it = tid_set.begin(); it != tid_set.end(); ++it)
      printf("process %d: detected %d threads\n", it->first, (int) it->second.size());

    for (int i = 1; i < argc; ++i)
      if (!strcmp(argv[i], "-t"))
      {
        analyze_time(ops);
        return 0;
      }

    analyze(ops);

  } else
  {
    fprintf(stderr, "cannot open directory %s\n", filename);
    exit(1);
  }

  return 0;
}
