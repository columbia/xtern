/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#include "tern/runtime/record-scheduler.h"
#include "tern/runtime/clockmanager.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <algorithm>
#include <sched.h>
#include "tern/options.h"

using namespace std;
using namespace tern;

//#undef run_queue
//#define run_queue list<int>

//#define _DEBUG_RECORDER

#ifdef _DEBUG_RECORDER
#  define SELFCHECK  dump(cerr); selfcheck()
#  define dprintf(fmt...) do {                   \
     fprintf(stderr, "[%d] ", self());            \
     fprintf(stderr, fmt);                       \
     fflush(stderr);                             \
   } while(0)
#else
#  define SELFCHECK
#  define dprintf(fmt...) ;
#endif

#if 1
#define RRSchedulerCV RRScheduler
extern int idle_done;
static void *monitor(void *arg)
{
  RRSchedulerCV * sched = (RRSchedulerCV*) arg;
  while (!idle_done)
  {
    usleep(options::RR_skip_threshold * 1000);
    //dprintf( "checking zombie\n");
    sched->detect_blocking_threads();
  }
  return 0;
}

extern pthread_mutex_t idle_mutex;
extern pthread_cond_t idle_cond;

pthread_mutex_t turn_mutex;

void RRSchedulerCV::detect_blocking_threads(void)
{
/*
 *  Each thread will update its timemark whenever getTurn or wakeup from wait,
 *  so that the value of timemark can be used to determine whether a thread has
 *  been in zombie state for a while.
 *
 *  If a thread is in zombie state, we simply skip its turn for once.
 */

#ifdef RRSchedulerCV
  //  we are using RRScheduler
  //  RRScheduler uses semaphore to pass turns. This algorithm is basically like
  //  a global token passing algorithm. What we do in the monitor is trying to 
  //  grab the global token, check liveness and repeat.

  int tid = -1;
#if 0
  //  the first solution is to check the list front, which is not thread safe,
  //  but is efficient.
  if (runq.size() && !sem_trywait(&waits[runq.front()].sem))
    tid = runq.front();
#else
  //  the second solution is to check all semaphore and grab the global token 
  //  if possible.
  int nt = nthread;
  for (int i = 0; i < nt; ++i)
    if (!sem_trywait(&waits[i].sem))
    {
      tid = i;
      break;
    }
#endif

  dprintf( "checking blocking threads\n");
  if (tid < 0)
  {
    dprintf( "checking blocking threads: first thread %d is enabled, so return\n",
      runq.size() ? runq.front() : -1);
    ++timer;
    return;
  }

  //  from this point, it should be thread safe, as we've grabbed the global token.
  assert(runq.size() && runq.front() == tid);
  int n = runq.size();
  bool find_live = false;
  while (runq.size() && n-- && !find_live)
  {
    int t = runq.front();
    if (timemark[t] >= 0 && timemark[t] < timer)
    {
      //  this means the thread enters user space before the previous check point. 
      //  put it another way, the thread didn't come back in the threshold. 

      //  so we skip it.
      runq.pop_front();
      runq.push_back(t);
      dprintf( "checking blocking threads: skip thread %d\n", t);
    } else
      find_live = true; //   this thread is either live or unknown.
  }

  if (!find_live) 
  {
    dprintf( "WARNING: all threads are blocked!!\n");
  }
  dprintf( "checking blocking threads: continue on thread %d\n", runq.front());
  sem_post(&waits[runq.front()].sem);

  ++timer;

#else
  pthread_mutex_lock(&lock);
  int n = runq.size() ;

  if (n > 0)
  {
    ++n;
    while (n--)
    {
      int tid = runq.front();
      if (timemark[tid] == timer || timemark[tid] == -1)
        break;
      runq.pop_front();
      runq.push_back(tid);
      dprintf( "we skipped %d\n", tid);
      SELFCHECK;
    }
  }
  ++timer;

  if(!runq.empty()) {
    int tid = runq.front();
    pthread_cond_signal(&waitcv[tid]);
  }
  pthread_mutex_unlock(&lock);
#endif

}
#undef RRSchedulerCV
#endif

void SeededRRScheduler::setSeed(unsigned seed)
{
  rand.srand(seed);
}

void SeededRRScheduler::reorderRunq(void)
{
  // find next thread using @rand.  complexity is O(runq.size()), but
  // shouldn't matter much anyway as the number of threads is small
  assert(!runq.empty());
  int i = rand.rand(runq.size()-1);
  assert(i >=0 && i < (int)runq.size() && "rand.rand() off bound");
  dprintf("SeededRRScheduler: reorder runq so %d is the front (size %d)\n",
          i, (int)runq.size());
  run_queue::iterator it = runq.begin();
  while(i--) ++it;
  assert(it != runq.end());
  int tid = *it;
  assert(tid >=0 && tid < Scheduler::nthread);
  runq.erase(it); // NOTE: this invalidates iterator @it!
  runq.push_front(tid);
}

void RRScheduler::wait_t::wait() {
  if (options::enforce_turn_type == "semaphore") {
    sem_wait(&sem);
  } else {
    /** by default, 3e4. This would cause the busy loop to loop for around 10 ms 
    on my machine, or 14 ms on bug00. This is one order of magnitude bigger
    than context switch time (1ms). **/
    /**
    2012-12-30: changed it to 4e4. By using a mencoder to convert a mpg file on bug00,
    using 4e4 only has 902 broken busy wait events, while using 3e4 has 13814.
    On bug00 the lib/runtime module is compiled with llvm optimized, I think.
    **/
    const long waitCnt = 4e4;
    volatile long i = 0;
    while (!wakenUp && i < waitCnt) {
      sched_yield();
      i++;
    }
    if (!wakenUp) {
      pthread_mutex_lock(&turn_mutex);
      while (!wakenUp) {/** This can save the context switch overhead. **/
        fprintf(stderr, "RRScheduler::wait_t::wait before cond wait, tid %d\n", self());
        pthread_cond_wait(&cond, &turn_mutex);
        fprintf(stderr, "RRScheduler::wait_t::wait after cond wait, tid %d\n", self());
      }
      wakenUp = false;
      pthread_mutex_unlock(&turn_mutex);
    } else {
      wakenUp = false;
    }
  }
}

void RRScheduler::wait_t::post() {
  if (options::enforce_turn_type == "semaphore") {
    sem_post(&sem);
  } else {
    pthread_mutex_lock(&turn_mutex);
    wakenUp = true;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&turn_mutex);
  }
}

//@before with turn
//@after with turn
unsigned RRScheduler::nextTimeout()
{
  unsigned next_timeout = FOREVER;
  list<int>::iterator i;
  for(i=waitq.begin(); i!=waitq.end(); ++i) {
    int t = *i;
    if(waits[t].timeout < next_timeout)
      next_timeout = waits[t].timeout;
  }
  return next_timeout;
}

//@before with turn
//@after with turn
int RRScheduler::fireTimeouts()
{
  int timedout = 0;
  list<int>::iterator prv, cur;
  // use delete-safe way of iterating the list
  for(cur=waitq.begin(); cur!=waitq.end();) {
    prv = cur ++;

    int tid = *prv;
    assert(tid >=0 && tid < Scheduler::nthread);
    if(waits[tid].timeout < turnCount) {
      dprintf("RRScheduler: %d timed out (%p, %u)\n",
              tid, waits[tid].chan, waits[tid].timeout);
      waits[tid].reset(ETIMEDOUT);
      waitq.erase(prv);
      runq.push_back(tid);
      ++ timedout;
    }
  }
  SELFCHECK;
  return timedout;
}

void RRScheduler::check_wakeup()
{
  static int check_count = 0;
  if (wakeup_flag && 
    (options::wakeup_period <= 0 || 
#if 1
    (int) turnCount >= (int) options::wakeup_period * check_count
#else
    !(turnCount % options::wakeup_period)
#endif
    ))
  {
    check_count = turnCount / options::wakeup_period + 1;
    pthread_mutex_lock(&wakeup_mutex);
    dprintf("check_wakeup works at turn %d\n", turnCount);
    dprintf("current runq = ");
    for (run_queue::iterator it = runq.begin(); it != runq.end(); ++it)
    {
      dprintf("%d ", *it);
    }
    dprintf("\n");
    dprintf("wakeup queue = ");
    for (int i = 0; i < (int) wakeup_queue.size(); ++i)
    {
      dprintf("%d ", wakeup_queue[i]);
    }
    dprintf("\n");
    sort(wakeup_queue.begin(), wakeup_queue.end()); //  TODO
    for (int i = 0; i < (int) wakeup_queue.size(); ++i)
      runq.push_back(wakeup_queue[i]);
    wakeup_queue.clear();
    wakeup_flag = false;
    pthread_mutex_unlock(&wakeup_mutex);
  }
}

//@before with turn
//@after with turn
void RRScheduler::next(bool at_thread_end, bool hasPoppedFront)
{
  int next_tid;
  if (!hasPoppedFront)
    runq.pop_front(); // remove self from runq

  check_wakeup();

  if(runq.empty()) {
    // Current thread must be the last thread and it is existing, otherwise we wake up the idle thread.
    if (at_thread_end && waitq.empty()) {
      return;
    } else if (at_thread_end && !waitq.empty() && self() == 0) {
      fprintf(stderr, "WARNING: main thread exits with some children threads alive (e.g., openmp).\n");
      return;
    } else if (options::launch_idle_thread && self() != IdleThreadTid) // If I am not the idle thread, then wake up the idle thread.
      wakeUpIdleThread();
  }

  assert(!runq.empty());
  reorderRunq();

  next_tid = runq.front();
  assert(next_tid>=0 && next_tid < Scheduler::nthread);
  dprintf("RRScheduler: next is %d\n", next_tid);
  SELFCHECK;
  waits[next_tid].post();
}

void RRScheduler::wakeUpIdleThread() {
  int idleTid = 1, tid = -1;
  list<int>::iterator prv, cur;
  // use delete-safe way of iterating the list
  for(cur=waitq.begin(); cur!=waitq.end();) {
    prv = cur ++;

    tid = *prv;
    assert(tid >=0 && tid < Scheduler::nthread);
    if(tid == idleTid) {
      waits[tid].reset();
      waitq.erase(prv);
      runq.push_back(tid);
      break;
    }
  }
  assert(tid == idleTid);
  pthread_mutex_lock(&idle_mutex);
  pthread_cond_signal(&idle_cond);
  pthread_mutex_unlock(&idle_mutex);
}

void RRScheduler::idleThreadCondWait() {
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  waits[tid].chan = (void *)&idle_cond;
  waits[tid].timeout = FOREVER;
  waitq.push_back(tid);
  assert(tid == runq.front());
  next();
  pthread_cond_wait(&idle_cond, &idle_mutex);
}

//@before without turn
//@after with turn
void RRScheduler::getTurn()
{
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  timemark[tid] = -1;  //  back to system space
  waits[tid].wait();
  dprintf("RRScheduler: %d gets turn\n", self());
  SELFCHECK;
}

int RRScheduler::block()
{
  getTurn();
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  assert(tid == runq.front());
  dprintf("RRScheduler: %d blocks\n", self());
  int ret = incTurnCount();
  next();
  return ret;
}

void RRScheduler::wakeup()
{
  pthread_mutex_lock(&wakeup_mutex);
  dprintf("thread %d wakes up at turn %d\n", self(), turnCount);
  wakeup_queue.push_back(self());
  wakeup_flag = true;
  pthread_mutex_unlock(&wakeup_mutex);
#if 0
  int tid = -1;
  dprintf("thread %d, wakeup start at turn %d\n", self(), getTurnCount());

  while (tid < 0)
  {
#if 0
  //  the first solution is to check the list front, which is not thread safe,
  //  but is efficient.
  if (runq.size() && !sem_trywait(&waits[runq.front()].sem))
    tid = runq.front();
#else
  //  the second solution is to check all semaphore and grab the global token 
  //  if possible.
  int nt = nthread;
  for (int i = 0; i < nt; ++i)
    if (!sem_trywait(&waits[i].sem))
    {
      tid = i;
      break;
    }
#endif
  }
  dprintf("thread %d, wakeup returned at turn %d\n", self(), getTurnCount());
  
  dprintf("RRScheduler: %d wakes up by stealing %d's turn\n", self(), tid);
  runq.push_back(self());
  timemark[self()] = timer;
  runq.push_front(self());  //  hack code
  next();
#endif
}

//@before with turn
//@after without turn
void RRScheduler::putTurn(bool at_thread_end)
{
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  assert(tid == runq.front());
  bool hasPoppedFront = false;

  timemark[tid] = timer;  //  return to user space at "timer"

  if(at_thread_end) {
    signal((void*)pthread_self());
    Parent::zombify(pthread_self());
    dprintf("RRScheduler: %d ends\n", self());
  } else {
    runq.pop_front();
    hasPoppedFront = true;
    runq.push_back(tid);
    dprintf("RRScheduler: %d puts turn\n", self());
  }

  next(at_thread_end, hasPoppedFront);
}

//@before with turn
//@after with turn
int RRScheduler::wait(void *chan, unsigned nturn)
{
  incTurnCount();
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  assert(tid == runq.front());
  waits[tid].chan = chan;
  waits[tid].timeout = nturn;
  waitq.push_back(tid);
  dprintf("RRScheduler: %d waits on (%p, %u)\n", tid, chan, nturn);

  next();

  timemark[tid] = -1; //  It's always ready to proceed when back to runq.

  getTurn();
  return waits[tid].status;
}

//@before with turn
//@after with turn
void RRScheduler::signal(void *chan, bool all)
{
  list<int>::iterator prv, cur;

  assert(chan && "can't signal/broadcast NULL");
  assert(self() == runq.front());
  dprintf("RRScheduler: %d: %s %p\n",
          self(), (all?"broadcast":"signal"), chan);

  // use delete-safe way of iterating the list in case @all is true
  for(cur=waitq.begin(); cur!=waitq.end();) {
    prv = cur ++;

    int tid = *prv;
    assert(tid >=0 && tid < Scheduler::nthread);
    if(waits[tid].chan == chan) {
      dprintf("RRScheduler: %d signals %d(%p)\n", self(), tid, chan);
      waits[tid].reset();
      waitq.erase(prv);
      runq.push_back(tid);
      if(!all)
        break;
    }
  }
  SELFCHECK;
}

extern int idle_done;
extern ClockManager clockManager;

//@before with turn
//@after with turn
unsigned RRScheduler::incTurnCount(void)
{
  unsigned ret = Serializer::incTurnCount();
  fireTimeouts();
  check_wakeup();
  clockManager.tick();
  return ret;
}

unsigned RRScheduler::getTurnCount(void)
{
  return Serializer::getTurnCount();
}

void RRScheduler::childForkReturn() {
  Parent::childForkReturn();
  for(int i=0; i<MAX_THREAD_NUM; ++i)
    waits[i].reset();
}


RRScheduler::~RRScheduler() {}

RRScheduler::RRScheduler()
{
  for(unsigned i=0; i<MAX_THREAD_NUM; ++i)
    sem_init(&waits[i].sem, 0, 0);

  // main thread
  assert(self() == MainThreadTid && "tid hasn't been initialized!");
  runq.createThreadElem(MainThreadTid);
  runq.push_back(self());
  waits[MainThreadTid].post();

  wakeup_queue.clear();
  wakeup_flag = 0;
  pthread_mutex_init(&wakeup_mutex, NULL);
  pthread_mutex_init(&turn_mutex, NULL);

  if (options::RR_skip_zombie)
  {
    timer = 0;
    memset(timemark, 0, sizeof(timemark));
    pthread_create(&monitor_th, NULL, monitor, this);
  } else
    monitor_th = 0;
}

void RRScheduler::selfcheck(void)
{
  fprintf(stderr, "RRScheduler::selfcheck tid %d\n", self());
  tr1::unordered_set<int> tids;

  // no duplicate tids on runq
  for(run_queue::iterator th=runq.begin(); th!=runq.end(); ++th) {
    if(*th < 0 || *th > Scheduler::nthread) {
      dump(cerr);
      assert(0 && "invalid tid on runq!");
    }
    if(tids.find(*th) != tids.end()) {
      dump(cerr);
      assert(0 && "duplicate tids on runq!");
    }
    tids.insert(*th);
  }

  // no duplicate tids on waitq
  for(list<int>::iterator th=waitq.begin(); th!=waitq.end(); ++th) {
    if(*th < 0 || *th > Scheduler::nthread) {
      dump(cerr);
      assert(0 && "invalid tid on waitq!");
    }
    if(tids.find(*th) != tids.end()) {
      dump(cerr);
      assert(0 && "duplicate tids on runq or waitq!");
    }
    tids.insert(*th);
  }

  // TODO: check that tids have all tids

  // threads on runq have NULL chan or non-forever timeout
  for(run_queue::iterator th=runq.begin(); th!=runq.end(); ++th)
    if(waits[*th].chan != NULL || waits[*th].timeout != FOREVER) {
      dump(cerr);
      assert(0 && "thread on runq but has non-NULL chan "\
             "or non-zero turns left!");
    }

  // threads on waitq have non-NULL waitvars or non-zero timeout
  for(list<int>::iterator th=waitq.begin(); th!=waitq.end(); ++th)
    if(waits[*th].chan == NULL && waits[*th].timeout == FOREVER) {
      dump(cerr);
      assert (0 && "thread on waitq but has NULL chan and 0 turn left!");
    }
}

ostream& RRScheduler::dump(ostream& o)
{
  o << "nthread " << Scheduler::nthread << ": " << turnCount;
  o << " [runq ";
  copy(runq.begin(), runq.end(), ostream_iterator<int>(o, " "));
  o << "]";
  o << " [waitq ";
  for(list<int>::iterator th=waitq.begin(); th!=waitq.end(); ++th)
    o << *th << "(" << waits[*th].chan << "," << waits[*th].timeout << ") ";
  o << "]\n";
  return o;
}

void FCFSScheduler::getTurn() {  
  pthread_mutex_lock(&fcfs_lock);
  //  fake
  runq.push_front(self());
}

void FCFSScheduler::putTurn(bool at_thread_end) {  
  int tid = self();
  assert(runq.size() && runq.front() == tid);

  if(at_thread_end) 
  {
    signal((void*)pthread_self());
    Parent::zombify(pthread_self());
  }

  next(at_thread_end);
}

FCFSScheduler::FCFSScheduler()
  : RRScheduler()
{  
  pthread_mutex_init(&fcfs_lock, NULL);

  assert(self() == MainThreadTid && "tid hasn't been initialized!");
  sem_init(&waits[MainThreadTid].sem, 0, 0);  //  clear it
}

FCFSScheduler::~FCFSScheduler() {  
  pthread_mutex_destroy(&fcfs_lock);
}

void FCFSScheduler::next(bool at_thread_end, bool hasPoppedFront)
{
  int tid = self();
  assert(runq.size() && runq.front() == tid);
  runq.pop_front();
  pthread_mutex_unlock(&fcfs_lock);
}

void FCFSScheduler::signal(void *chan, bool all)
{
  list<int>::iterator prv, cur;

  assert(chan && "can't signal/broadcast NULL");
  assert(self() == runq.front());
  dprintf("RRScheduler: %d: %s %p\n",
          self(), (all?"broadcast":"signal"), chan);

  // use delete-safe way of iterating the list in case @all is true
  for(cur=waitq.begin(); cur!=waitq.end();) {
    prv = cur ++;

    int tid = *prv;
    assert(tid >=0 && tid < Scheduler::nthread);
    if(waits[tid].chan == chan) {
      dprintf("RRScheduler: %d signals %d(%p)\n", self(), tid, chan);
      waits[tid].reset();
      waitq.erase(prv);
      //runq.push_back(tid);

      sem_post(&waits[tid].sem); //  added for FCFSScheduler
      if(!all)
        break;
    }
  }
  SELFCHECK;
}

int FCFSScheduler::wait(void *chan, unsigned nturn)
{
  incTurnCount();
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  assert(tid == runq.front());
  waits[tid].chan = chan;
  waits[tid].timeout = nturn;
  waitq.push_back(tid);

  next();

  sem_wait(&waits[tid].sem); // added from RRScheduler's wait()

  getTurn();
  return waits[tid].status;
}

