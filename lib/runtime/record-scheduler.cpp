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
#include "tern/options.h"

using namespace std;
using namespace tern;

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

#if 0
void RRSchedulerCV::threadEnd(pthread_t self_th) {
  assert(self() == runq.front());

  pthread_mutex_lock(&lock);
  signalHelper((void*)self_th, OneThread, NoLock, NoUnlock);
  next();
  Parent::threadEnd(self_th);
  pthread_mutex_unlock(&lock);
}

RRSchedulerCV::~RRSchedulerCV()
{
  //if (monitor_th) { pthread_cancel(monitor_th); monitor_th = 0; }
  pthread_cond_destroy(&replaycv);
  pthread_cond_destroy(&tickcv);
  if (log)
  {
    fflush(log);
    fclose(log);
    log = NULL;
  }
}

RRSchedulerCV::RRSchedulerCV(pthread_t main_th): Parent(main_th) {
  pthread_mutex_init(&lock, NULL);
  for(unsigned i=0; i<MaxThreads; ++i) {
    pthread_cond_init(&waitcv[i], NULL);
    waitvar[i] = NULL;
  }
  pthread_cond_init(&replaycv, NULL);
  pthread_cond_init(&tickcv, NULL);

  assert(self() == MainThreadTid && "tid hasn't been initialized!");
  runq.push_back(self()); // main thread

  log = fopen("message.log", "w");
  assert(log && "open message log file failed!");

#if 1
  ifstream inf("replay.log");
  if (inf.good())
  {
    int tid, turn;
    while (inf >> tid >> turn)
    {
      if (turn < 0) continue;
      net_item item;
      item.tid = tid;
      item.turn = turn;
      net_events.push_back(item);
    }
    inf.close();
  }
#else
  FILE *fin = fopen("replay.log", "r");
  if (fin)
  {
    int tid, turn;
    //while (fscanf("%d %d", &tid, &turn) == 2)
    while (fscanf(fin, "%d %d", &tid, &turn) == 2)
    {
      net_item item;
      item.tid = tid;
      item.turn = turn;
      net_events.push_back(item);
    }
    fclose(fin);
  }
#endif
  if (options::RR_skip_zombie)
  {
    timer = 0;
    memset(timemark, 0, sizeof(timemark));
    pthread_create(&monitor_th, NULL, monitor, this);
  } else
    monitor_th = 0;
}

/// check if current thread is head of runq; otherwise, wait on the cond
/// var assigned for the current thread.  It does not acquire or release
/// this->lock
void RRSchedulerCV::getTurnHelper(bool doLock, bool doUnlock) {
  int tid = self();
  timemark[tid] = -1;

  if(doLock)
    pthread_mutex_lock(&lock);

  assert(tid>=0 && tid<MaxThreads);
  for(;;) {
    if (!net_events.empty() && (int) net_events.begin()->turn == (int) turnCount)
    {
      pthread_cond_wait(&replaycv, &lock);
      continue;
    }
    if (tid != runq.front())
      pthread_cond_wait(&waitcv[tid], &lock);
    else
      break;
  }

  SELFCHECK;
  //cerr << "RRScheduler: " << tid << " get turn." << endl;
  dprintf("RRSchedulerCV: %d: got turn\n", self());

  if(doUnlock)
    pthread_mutex_unlock(&lock);
}

/// move head of runq to tail, and signals the current head.  It does not
/// acquire or release this->lock
void RRSchedulerCV::putTurnHelper(bool doLock, bool doUnlock) {
  if(doLock)
    pthread_mutex_lock(&lock);

  runq.push_back(self());
  next();
  SELFCHECK;

  timemark[self()] = timer;

  if(doUnlock)
    pthread_mutex_unlock(&lock);
}

/// move current head of runq to tail of waitq, and block current thread
/// on its dedicated cond var.
void RRSchedulerCV::waitHelper(void *chan, bool doLock, bool doUnlock) {

  waitFirstHalf(chan, doLock);

  int tid = self();
  while(waitvar[tid])
    pthread_cond_wait(&waitcv[tid], &lock);

  dprintf("RRSchedulerCV: %d: wake up from %p\n", self(), chan);

  if(doUnlock)
    pthread_mutex_unlock(&lock);
}

/// move current head of runq to tail of waitq
void RRSchedulerCV::waitFirstHalf(void *chan, bool doLock) {
  assert(chan && "can't wait on NULL");
  assert(self() == runq.front());

  if(doLock)
    pthread_mutex_lock(&lock);

  dprintf("RRSchedulerCV: %d: waiting (%p)\n", self(), chan);

  int tid = self();
  assert(waitvar[tid]==NULL && "thread already waits!");
  waitvar[tid] = chan;
  waitq.push_back(tid);
  next();

  SELFCHECK;
}

bool RRSchedulerCV::isWaiting() {
  return waitvar[self()] != NULL;
}

/// pop current runq head, and signal new head.  must call after the
/// current thread has finished moving threads to runq
void RRSchedulerCV::next(void) {
  int tid = self();
  assert(tid == runq.front());
  runq.pop_front();
  if(!runq.empty()) {
    int tid = runq.front();
    assert(tid>=0 && tid < Scheduler::nthread);
    pthread_cond_signal(&waitcv[tid]);
    dprintf("RRSchedulerCV: %d gives turn to %d\n", self(), tid);
    SELFCHECK;
  }
}

/// same as signal() but does not acquire the scheduler lock; if @all is
/// true, signal all threads waiting on @chan
void RRSchedulerCV::signalHelper(void *chan, bool all,
                                 bool doLock, bool doUnlock, bool wild) {
  list<int>::iterator prv, cur;

  if(doLock)
    pthread_mutex_lock(&lock);

  assert(chan && "can't signal/broadcast NULL");
  assert(!wild || self() == runq.front());
  dprintf("RRSchedulerCV: %d: %s %p\n",
          self(), (all?"broadcast":"signal"), chan);

  // use delete-safe way of iterating the list in case @all is true
  for(cur=waitq.begin(); cur!=waitq.end();) {
    prv = cur ++;

    int tid = *prv;
    assert(tid >=0 && tid < Scheduler::nthread);
    if(waitvar[tid] == chan) {
      waitvar[tid] = NULL;
      waitq.erase(prv);
      runq.push_back(tid);
      pthread_cond_signal(&waitcv[tid]);
      dprintf("RRSchedulerCV: %d: signaled %d(%p)\n", self(), tid, chan);
      if(!all)
        break;
    }
  }
  SELFCHECK;

  if(doUnlock)
    pthread_mutex_unlock(&lock);
}

void RRSchedulerCV::selfcheck(void) {
  tr1::unordered_set<int> tids;

  // no duplicate tids on runq
  for(list<int>::iterator th=runq.begin(); th!=runq.end(); ++th) {
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
      assert(0 && "duplicate tids on waitq!");
    }
    tids.insert(*th);
  }

  // threads on runq have NULL waitvars
  for(list<int>::iterator th=runq.begin(); th!=runq.end(); ++th)
    if(waitvar[*th] != NULL) {
      dump(cerr);
      assert(0 && "thread on runq but has non NULL wait var!");
    }

  // threads on waitq have non-NULL waitvars
  for(list<int>::iterator th=waitq.begin(); th!=waitq.end(); ++th)
    if(waitvar[*th] == NULL) {
      dump(cerr);
      assert (0 && "thread on waitq but has NULL wait var!");
    }
}

ostream& RRSchedulerCV::dump(ostream& o) {
  o << "nthread " << Scheduler::nthread << ": " << turnCount;
  o << " [runq ";
  copy(runq.begin(), runq.end(), ostream_iterator<int>(o, " "));
  o << "]";
  o << " [waitq ";
  for(list<int>::iterator th=waitq.begin(); th!=waitq.end(); ++th)
    o << *th << "(" << waitvar[*th] << ") ";
  o << "]\n";
  return o;
}

void RRSchedulerCV::block()
{
  if (!options::schedule_network) return;

/*
  pthread_mutex_lock(&lock);

  assert(tid>=0 && tid<MaxThreads);
  while(tid != runq.front())
    pthread_cond_wait(&waitcv[tid], &lock);
*/
  getTurn();
  unsigned ret = incTurnCount();

  assert(log && "open message log file failed!");
  fprintf(log, "%d %d\n", (int)self(), - ret - 1);
  fflush(log);

  //  like putTurn
  // I'm blocked! Don't push me into the queue!
  //runq.push_back(self());
  next();
  SELFCHECK;
}

void RRSchedulerCV::wakeup()
{
  if (!options::schedule_network) return;

  int tid = self();

  pthread_mutex_lock(&lock);

  for(;;) {
    if (net_events.empty())
    {
      //  the recording part
      runq.push_back(tid);
      SELFCHECK;
      pthread_mutex_unlock(&lock);

      getTurn();  //  ensure atomicity
      break;
    }

    //  the replay part
    if (net_events.begin()->tid != tid)
      pthread_cond_wait(&replaycv, &lock);
    else {
      if (net_events.begin()->turn < (int)turnCount)
      {
        // int t = net_events.front().turn;
        // int id = net_events.front().tid;
        exit(-1);
      }

      if (net_events.begin()->turn == (int) turnCount)
      {
        net_events.pop_front();
        pthread_cond_broadcast(&replaycv);
        break;
      }
      pthread_cond_wait(&tickcv, &lock);
    }
  }

  assert(tid>=0 && tid<MaxThreads);

  assert(log && "open message log file failed!");
  unsigned ret = incTurnCount();
  fprintf(log, "%d %d\n", tid, ret);
  fflush(log);
  //cerr << tid << ' ' << ret << endl;

  if (runq.size() && tid == runq.front()) // the record part or some replay part
    putTurn();
  else
  {
    runq.push_back(self());
    SELFCHECK;
    if(!runq.empty()) {
      int tid = runq.front();
      assert(tid>=0 && tid < Scheduler::nthread);
      pthread_cond_signal(&waitcv[tid]);
      dprintf("RRSchedulerCV: %d gives turn to %d\n", self(), tid);
    }
    pthread_mutex_unlock(&lock);
  }
}
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
  list<int>::iterator it = runq.begin();
  while(i--) ++it;
  assert(it != runq.end());
  int tid = *it;
  assert(tid >=0 && tid < Scheduler::nthread);
  runq.erase(it); // NOTE: this invalidates iterator @it!
  runq.push_front(tid);
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
    turnCount >= options::wakeup_period * check_count
#else
    !(turnCount % options::wakeup_period)
#endif
    ))
  {
    check_count = turnCount / options::wakeup_period + 1;
    pthread_mutex_lock(&wakeup_mutex);
    dprintf("check_wakeup works at turn %d\n", turnCount);
    dprintf("current runq = ");
    for (list<int>::iterator it = runq.begin(); it != runq.end(); ++it)
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
void RRScheduler::next(bool at_thread_end)
{
  int next_tid;
  runq.pop_front(); // remove self from runq

  check_wakeup();

  if(runq.empty()) {
    // if there are any timed-waiting threads, we can fast forward the
    // turn to wake up these threads
    unsigned next_timeout = nextTimeout();
    if(next_timeout != FOREVER) {
      // fast-forward turn
      dprintf("RRScheduler: fast-forward turn from %u to %u\n",
              turnCount, next_timeout+1);
      turnCount = next_timeout + 1;
      fireTimeouts();
    }

    if(runq.empty()) {
      // current thread must be the last thread and it is existing
      assert(at_thread_end && waitq.empty()
             && "all threads wait; deadlock!");
      return;
    }
  }

  assert(!runq.empty());
  reorderRunq();

  next_tid = runq.front();
  assert(next_tid>=0 && next_tid < Scheduler::nthread);
  dprintf("RRScheduler: next is %d\n", next_tid);
  SELFCHECK;
  sem_post(&waits[next_tid].sem);
}

//@before without turn
//@after with turn
void RRScheduler::getTurn()
{
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  timemark[tid] = -1;  //  back to system space
  sem_wait(&waits[tid].sem);
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
  int ret = getTurnCount();
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

  timemark[tid] = timer;  //  return to user space at "timer"

  if(at_thread_end) {
    signal((void*)pthread_self());
    Parent::zombify(pthread_self());
    dprintf("RRScheduler: %d ends\n", self());
  } else {
    runq.push_back(tid);
    dprintf("RRScheduler: %d puts turn\n", self());
  }

  next(at_thread_end);
}

//@before with turn
//@after with turn
int RRScheduler::wait(void *chan, unsigned nturn)
{
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
  unsigned ret = turnCount++;
  fireTimeouts();
  check_wakeup();
  clockManager.tick();
  return idle_done ? (1 << 30) : ret;
}

unsigned RRScheduler::getTurnCount(void)
{
  return idle_done ? (1 << 30) : turnCount - 1;
}

void RRScheduler::childForkReturn() {
  Parent::childForkReturn();
  for(int i=0; i<MaxThreads; ++i)
    waits[i].reset();
}


RRScheduler::RRScheduler()
{
  for(unsigned i=0; i<MaxThreads; ++i)
    sem_init(&waits[i].sem, 0, 0);

  // main thread
  assert(self() == MainThreadTid && "tid hasn't been initialized!");
  runq.push_back(self());
  sem_post(&waits[MainThreadTid].sem);

  wakeup_queue.clear();
  wakeup_flag = 0;
  pthread_mutex_init(&wakeup_mutex, NULL);

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
  tr1::unordered_set<int> tids;

  // no duplicate tids on runq
  for(list<int>::iterator th=runq.begin(); th!=runq.end(); ++th) {
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
  for(list<int>::iterator th=runq.begin(); th!=runq.end(); ++th)
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
