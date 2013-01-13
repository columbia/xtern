/* Author: Huayang Guo (huayang@cs.columbia.edu) -*- Mode: C++ -*- */
// Modified from record-scheduler.cpp.

#include "tern/runtime/record-scheduler.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include "tern/options.h"
#include "tern/space.h"

using namespace std;
using namespace tern;

#if 1
#define dprintf(...) { fprintf(stderr, __VA_ARGS__); }
#else
#define dprintf(...) { }
#endif

static vector<string> split(char *p, const char *del = " ")
{
  string st = p;
  vector<string> ret;
  int last = -1;
  while (last < (int) st.size())
  {
    int i = st.find_first_of(del, last + 1);
    if (i == (int) string::npos) i = (int) st.size(); 
    if (i > last + 1)
      ret.push_back(st.substr(last + 1, i - last - 1));
    last = i;
  }

  return ret;
}

void ReplaySchedulerSem::readrecords(FILE * fin, record_list &records)
{
  records.clear();

  //  the first line determines the log format
  const int buffer_length = 1024;
  char buffer[buffer_length];
  buffer[0] = 0;
  fgets(buffer, buffer_length, fin); 
  vector<string> keys = split(buffer);

  while (fgets(buffer, buffer_length, fin))
  {
    istringstream is(buffer);
    record_type r;
    for (int i = 0; i < (int) keys.size(); ++i)
      if (keys[i] == "args")
      {
        is.getline(buffer, buffer_length);
        r[keys[i]] = buffer;
        break;
      } else
        is >> r[keys[i]];
    records.push_back(r);
  }
}

ReplaySchedulerSem::ReplaySchedulerSem()
{
  //  initialize member variables
  for (int i = 0; i < MAX_THREAD_NUM; ++i)
    //  keep token at the main thread at the begin
    sem_init(&waits[i], 0, i == 0);   

  //  read log files from options::replay_log_dir
  string log_dir = options::replay_log_dir;

  logdata.clear();
  if (options::replay_log_type == "recorder")
  {
    for (int i = 0; ; ++i)
    {
      char buffer[256];
      sprintf(buffer, "%s/tid-%d.txt", log_dir.c_str(), i);
      FILE *fin = fopen(buffer, "r");
  
      //  maybe we want to check errno here in case the file is busy
      if (!fin)
        break;
  
      logdata.push_back(record_list());
      record_list &records = logdata.back();
  
      readrecords(fin, records);
  
      fclose(fin);
    }
  } else if (options::replay_log_type == "serializer")
  {
    FILE *fin = fopen("replay.log", "r");

    char tidst[256];
    char buffer[256];
    int tid, turn;
    while (fscanf(fin, "%d %d", &tid, &turn) > 0)
    {
      sprintf(tidst, "%d", tid);
      sprintf(buffer, "%d", turn);

      while ((int) logdata.size() <= tid) 
        logdata.push_back(record_list());
      record_type r;
      r["op"] = "unknown";
      r["turn"] = string(buffer);
      r["tid"] = string(tidst);
      logdata[tid].push_back(r);
    }
    fclose(fin);
  } else
    assert(false && "unrecognized replay log type");

  dprintf("obtained %d thread-logs\n", (int) logdata.size());
}

ReplaySchedulerSem::~ReplaySchedulerSem()
{
}

void ReplaySchedulerSem::getTurn()
{
  sem_wait(&waits[self()]);
  record_type &r = logdata[self()].next();
  int real_turn = getTurnCount() + 1;
  //if (real_turn > INF) real_turn = INF; 

  if (r["op"] != "tern_idle" && 0)
  {
    fprintf(stderr, "thread %d: op = %s, turn = %s, runtime_turnCount = %d\n", 
    self(), r["op"].c_str(), r["turn"].c_str(), real_turn);
    fflush(stderr);
  }
  int log_turn = atoi(r["turn"].c_str());
  assert(log_turn == real_turn && "log turn number and real turn number inconsistent");
  logdata[self()].move_next();
}

void ReplaySchedulerSem::putTurn(bool at_thread_end)
{
  if(at_thread_end) {
    zombify(pthread_self());
  }

  //  find the next scheduled thread and post semaphore
  int next_turn = -1;
  int next_tid = -1;
  for (int i = 0; i < (int) logdata.size(); ++i)
    if (logdata[i].has_next())
    {
      int turn = atoi(logdata[i].next()["turn"].c_str());
      if (next_turn < 0 || next_turn > turn)
      {
        next_turn = turn;
        next_tid = i;
      }
    }
  
  if (next_tid < 0)
  {
    if (t_p_map.size())
    {
      dprintf("warning: next_tid not found, will give control to a random thread\n"
              "\tThis is dangarous and might lead to deadlock\n");
      while (t_p_map.find(next_tid) == t_p_map.end())
        next_tid = rand() % nthread;
    } else {
      dprintf("warning: t_p_map is empty, assume here's the end of execution\n"
              "\tNow continue without posting semaphore\n");
      return;
    }
  }

  sem_post(&waits[next_tid]);
}

