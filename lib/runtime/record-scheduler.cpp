/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#include "tern/runtime/record-scheduler.h"
#include <iostream>
#include <iostream>
#include <iterator>
#include <vector>

using namespace std;
using namespace tern;

//#define _DEBUG_RECORDER

#ifdef _DEBUG_RECORDER
#  define SELFCHECK  dump(cerr); selfcheck()
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define SELFCHECK
#  define dprintf(fmt...)
#endif

void RRSchedulerCV::threadEnd(pthread_t self_th) {
  assert(self() == runq.front());

  pthread_mutex_lock(&lock);
  signalHelper((void*)self_th, OneThread, NoLock, NoUnlock);
  next();
  Parent::threadEnd(self_th);
  pthread_mutex_unlock(&lock);
}

RRSchedulerCV::RRSchedulerCV(pthread_t main_th): Parent(main_th) {
  pthread_mutex_init(&lock, NULL);
  for(unsigned i=0; i<MaxThreads; ++i) {
    pthread_cond_init(&waitcv[i], NULL);
    waitvar[i] = NULL;
  }
  assert(self() == MainThreadTid && "tid hasn't been initialized!");
  runq.push_back(self()); // main thread
}

/// check if current thread is head of runq; otherwise, wait on the cond
/// var assigned for the current thread.  It does not acquire or release
/// this->lock
void RRSchedulerCV::getTurnHelper(bool doLock, bool doUnlock) {
  int tid = self();

  if(doLock)
    pthread_mutex_lock(&lock);

  assert(tid>=0 && tid<MaxThreads);
  while(tid != runq.front())
    pthread_cond_wait(&waitcv[tid], &lock);

  SELFCHECK;
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

  dprintf("RRSchedulerCV: %d: waiting (no CV) on %p\n", self(), chan);

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

  assert(chan && "can't signal/broadcast  NULL");
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
  o << "nthread " << Scheduler::nthread;
  o << " [runq ";
  copy(runq.begin(), runq.end(), ostream_iterator<int>(o, " "));
  o << "]";
  o << " [waitq ";
  for(list<int>::iterator th=waitq.begin(); th!=waitq.end(); ++th)
    o << *th << "(" << waitvar[*th] << ") ";
  o << "]\n";
  return o;
}



