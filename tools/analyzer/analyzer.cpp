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

using namespace std;
using namespace tern;

bool compute_signature = 0;
bool po_signature = false;
bool bdb_spec = false;
uint64_t turn_limit = ((uint64_t) 1) << 60;


extern void build_barrier_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow);
extern void build_create_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow);
extern void build_mutex_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow);
extern void build_sem_hb(vector<op_t> &ops, vector<vector<int> > &hb_arrow);

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
  if (x.rec.turn == y.rec.turn)
  {
    if (x.pid != y.pid)
      return x.pid < y.pid;
    return x.rec.tid > y.rec.tid;
  }
  return x.rec.turn < y.rec.turn;
}

void usage(int argc,  char *argv[])
{
  fprintf(stderr, "Usage\n\t%s <log directory>\n", argv[0]);
  exit(1);
}

vector<op_t> ops;
vector<op_t> ops2;
extern void print_signature(vector<op_t> &ops, vector<vector<int> > &hb_arrow);
extern void print_trace(vector<op_t> &ops, vector<vector<int> > &hb_arrow);
extern void comp_log(
  vector<op_t> &x, const char *dir1, vector<op_t> &y, const char *dir2);

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
      if (aptcol.size()) { fprintf(ouf, "%lld", (long long) aptcol.back()); aptcol.pop_back(); } 
      fprintf(ouf, ", "); 
      if (sytcol.size()) { fprintf(ouf, "%lld", (long long) sytcol.back()); sytcol.pop_back(); } 
      fprintf(ouf, ", "); 
      if (sctcol.size()) { fprintf(ouf, "%lld", (long long) sctcol.back()); sctcol.pop_back(); } 
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
    printf(" %09lld", (long long) (rec.sum_##x / rec.count)); \
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

  if (!compute_signature)
    print_trace(ops, hb_arrow);
  else
    print_signature(ops, hb_arrow);
}

void readlog(const char *filename, vector<op_t> &ops)
{
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
        if (ops.back().rec.turn >= turn_limit)
          ops.pop_back();
        else
        if (syncfunc::isBlockingSyscall(ops.back().rec.op))
          ops.pop_back(); //  turn # of block syscall is non-det

        switch (ops.back().rec.op)
        {
        case (syncfunc::tern_thread_begin):
        case (syncfunc::tern_thread_end): 
        case (syncfunc::pthread_mutex_lock):
        case (syncfunc::pthread_mutex_unlock):
        case (syncfunc::pthread_mutex_trylock):
        case (syncfunc::pthread_mutex_timedlock):
        case syncfunc::pthread_cond_signal:
        case syncfunc::pthread_cond_wait:
        case syncfunc::pthread_cond_broadcast:
        case syncfunc::pthread_cond_timedwait:
        case syncfunc::pthread_barrier_wait:
        case syncfunc::sem_wait:
        case syncfunc::sem_post:
        case syncfunc::sem_trywait:
        case syncfunc::sem_timedwait:
        case syncfunc::pthread_join:
        case syncfunc::pthread_create:
        case syncfunc::pthread_rwlock_wrlock:
        case syncfunc::pthread_rwlock_rdlock:
        case syncfunc::pthread_rwlock_tryrdlock:
        case syncfunc::pthread_rwlock_trywrlock:
        case syncfunc::pthread_rwlock_unlock:
          break;
        default:
          ops.pop_back(); //  turn # of block syscall is non-det
        }
      }

      delete reader;
    }

    printf("collected %d records from %d processes and %d log files.\n", 
      (int) ops.size(), (int) pid_set.size(), log_file_count);
    for (map<int, set<int> >::iterator it = tid_set.begin(); it != tid_set.end(); ++it)
      printf("process %d: detected %d threads\n", it->first, (int) it->second.size());
  } else
  {
    fprintf(stderr, "cannot open directory %s\n", filename);
    exit(1);
  }

 
}

int main(int argc, char *argv[])
{
  if (argc < 2)
    usage(argc, argv);

  const char *filename = argv[1];
  const char *dir2 = NULL;
  bool comp_dir = false;
  bool anay_time = false;

  for (int i = 1; i < argc; ++i)
  {
    if (!strcmp(argv[i], "-t"))
    {
      anay_time = true;
    } else
    if (!strcmp(argv[i], "-cmp"))
    {
      comp_dir = true;
      ++i;
      if (i < argc)
        dir2 = argv[i];
    } else
    if (!strcmp(argv[i], "-bdb"))
    {
      bdb_spec = true;
    } else
    if (!strcmp(argv[i], "-po"))
    {
      po_signature = true;
    } else
    if (!strcmp(argv[i], "-tlimit"))
    {
      ++i; 
      if (i < argc)
        turn_limit = atoi(argv[i]);
      continue;
    } else
    if (!strcmp(argv[i], "-c"))
    {
      compute_signature = 1;
    }
  }
  
  readlog(filename, ops);

  if (anay_time)
  {
    analyze_time(ops);
    return 0;
  }

  if (comp_dir)
  {
    readlog(dir2, ops2);
    comp_log(ops, filename, ops2, dir2);
    return 0;
  }

  analyze(ops);

 return 0;
}
