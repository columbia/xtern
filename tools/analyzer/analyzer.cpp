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
  int get_int(int idx)
  {
    if (idx >= 0 && idx < (int) rec.args.size())
      return atoi(rec.args[idx].c_str());
    else
      return -1;
  }
};

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
//    case syncfunc::pthread_mutex_lock:
//    case syncfunc::pthread_mutex_unlock:
//    case syncfunc::pthread_cond_signal:
//    case syncfunc::pthread_cond_broadcast:
//    case syncfunc::sem_wait:
//    case syncfunc::sem_timedwait:
//    case syncfunc::sem_post:
//    case syncfunc::pthread_cond_wait:
//    case syncfunc::pthread_cond_timedwait:
//    case syncfunc::pthread_mutex_trylock:
//    case syncfunc::sem_trywait:
//    case syncfunc::pthread_create:
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

  for (int i = 0; i < n;++i)
  {
    switch (ops[i].rec.op)
    {
    case syncfunc::pthread_create:
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

    analyze(ops);

  } else
  {
    fprintf(stderr, "cannot open directory %s\n", filename);
    exit(1);
  }

  return 0;
}
