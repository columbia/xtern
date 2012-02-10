/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#include "tern/runtime/record-scheduler.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <stdlib.h>
#include "tern/options.h"

using namespace std;
using namespace tern;

//#define _DEBUG_RECORDER

#ifdef _DEBUG_RECORDER
#  define SELFCHECK  dump(cerr); selfcheck()
#  define dprintf(fmt...) fprintf(stderr, fmt); fflush(stderr);
#else
#  define SELFCHECK
#  define dprintf(fmt...)
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
}

/// check if current thread is head of runq; otherwise, wait on the cond
/// var assigned for the current thread.  It does not acquire or release
/// this->lock
void RRSchedulerCV::getTurnHelper(bool doLock, bool doUnlock) {
  int tid = self();

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
  pthread_mutex_lock(&lock);
  // I'm blocked! Don't push me into the queue!
  //runq.push_back(self());
  next();
  SELFCHECK;

  pthread_mutex_unlock(&lock);
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
  int i = rand.rand(runq.size());
  list<int>::iterator it = runq.begin();
  while(i--) ++it;
  assert(it != runq.end());
  runq.erase(it);
  runq.push_front(*it);
}

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

void RRScheduler::next(bool at_thread_end)
{
  int next_tid;
  runq.pop_front(); // remove self from runq
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

  reorderRunq();

  next_tid = runq.front();
  assert(next_tid>=0 && next_tid < Scheduler::nthread);
  dprintf("RRScheduler: next is %d\n", next_tid);
  SELFCHECK;
  sem_post(&waits[next_tid].sem);
}

void RRScheduler::getTurn()
{
  int tid = self();
  assert(tid>=0 && tid < Scheduler::nthread);
  sem_wait(&waits[tid].sem);
  dprintf("RRScheduler: %d gets turn\n", self());
  SELFCHECK;
}

void RRScheduler::putTurn(bool at_thread_end)
{
  int tid = self();
  assert(tid == runq.front());

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

int RRScheduler::wait(void *chan, unsigned nturn)
{
  int tid = self();
  assert(tid == runq.front());
  waits[tid].chan = chan;
  waits[tid].timeout = nturn;
  waitq.push_back(tid);
  dprintf("RRScheduler: %d waits on (%p, %u)\n", tid, chan, nturn);

  next();

  getTurn();
  return waits[tid].status;
}

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

unsigned RRScheduler::incTurnCount(void)
{
  unsigned ret = turnCount++;

  fireTimeouts();

#if 0
  list<int>::iterator prv, cur;

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
    }
  }
#endif

  return ret;
}


RRScheduler::RRScheduler()
{
  for(unsigned i=0; i<MaxThreads; ++i)
    sem_init(&waits[i].sem, 0, 0);

  // main thread
  assert(self() == MainThreadTid && "tid hasn't been initialized!");
  runq.push_back(self());
  sem_post(&waits[MainThreadTid].sem);
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
