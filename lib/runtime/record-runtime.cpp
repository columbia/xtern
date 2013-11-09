/* Copyright (c) 2013,  Regents of the Columbia University 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other 
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Authors: Heming Cui (heming@cs.columbia.edu), Junfeng Yang (junfeng@cs.columbia.edu).  Refactored from
// Heming's Memoizer code
//
// Some tricky issues addressed or discussed here are:
//
// 1. deterministic pthread_create
// 2. deterministic and deadlock-free pthread_barrier_wait
// 3. deterministic and deadlock-free pthread_cond_wait
// 4. timed wait operations (e.g., pthread_cond_timewait)
// 5. try-operations (e.g., pthread_mutex_trylock)
//
// TODO:
// 1. implement the other proposed solutions to pthread_cond_wait
// 2. design and implement a physical-to-logical mapping to address timeouts
// 3. implement replay runtime
//
// timeout based on real time is inherently nondeterministic.  three ways
// to solve
//
// - ignore.  drawback: changed semantics.  program may get stuck (e.g.,
//   pbzip2) if they rely on timeout to move forward.
//
// - record timeout, and try to replay (tern approach)
//
// - replace physical time with logical number of sync operations.  can
//   count how many times we get the turn or the turn changed hands, and
//   timeout based on this turn count.  This approach should maintain
//   original semantics if program doesn't rely on physical running time
//   of code.
//
//   optimization, if there's a deadlock (activeq == empty), we just wake
//   up timed waiters in order
//

#include <sys/time.h>
#include <execinfo.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include "tern/runtime/record-log.h"
#include "tern/runtime/record-runtime.h"
#include "tern/runtime/record-scheduler.h"
#include "signal.h"
#include "helper.h"
#include "tern/space.h"
#include "tern/options.h"
#include "tern/hooks.h"
#include "tern/runtime/rdtsc.h"

#include <fstream>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

// FIXME: these should be in tern/config.h
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE<199309L)
// Need this for clock_gettime
#  define _POSIX_C_SOURCE (199309L)
#endif

//#define _DEBUG_RECORDER

#ifdef _DEBUG_RECORDER
#  define dprintf(fmt...) do {                   \
     fprintf(stderr, "[%d] ", _S::self());        \
     fprintf(stderr, fmt);                       \
     fflush(stderr);                             \
  } while(0)
#else
#  define dprintf(fmt...)
#endif

//#define __TIME_MEASURE 1

#define ts2ll(x) ((uint64_t) (x).tv_sec * factor + (x).tv_nsec)

using namespace std;

/// Variables for the non-det prfimitive.
pthread_cond_t nonDetCV; /** This cond var does not actually work with other mutexes to do
                                        real cond wait and signal, it just provides a cond var addr for 
                                        _S::wait() and _S::signal(). **/
__thread volatile bool inNonDet = false; /** Per-thread variable to denote whether current thread is within a non_det region. **/
int nNonDetWait = 0; /** This variable is only accessed when a thread gets a turn, so it is safe. **/
tr1::unordered_set<void *> nonDetSyncs; /** Global set to store the sync vars that have ever been accessed within non_det regions of all threads. **/
pthread_spinlock_t nonDetLock; /** a spinlock to protect the acccess to the global set "nonDetSyncs". **/

/** An internal function to record a set of sync vars accessed in non-det regions. **/
void add_non_det_var(void *var) {
  /*pthread_spin_lock(&nonDetLock);
  nonDetSyncs.insert(var);
  pthread_spin_unlock(&nonDetLock);*/
}
 
/** An internal function to check a sync var has not been accessed in non-det regions. **/
bool is_non_det_var(void *var) {
  return false;
  /*pthread_spin_lock(&nonDetLock);
  bool ret = (nonDetSyncs.find(var) != nonDetSyncs.end());
  pthread_spin_unlock(&nonDetLock);
  if (ret)
    fprintf(stderr, "WARN: NON-DET SYNC VAR IS ACCESSED IN DETERMINISTIC REGION.\n");  
  return ret;*/
}

namespace tern {

extern "C" {
  extern int idle_done;
}
static __thread timespec my_time;

extern "C" void *idle_thread(void*);
extern "C" pthread_t idle_th;
extern "C" pthread_mutex_t idle_mutex;
extern "C" pthread_cond_t idle_cond;

/** This var works with tern_set_base_time(). It is used to record the base 
time for cond_timedwait(), sem_timedwait() and mutex_timedlock() to get
deterministic physical time interval, so that this interval can be 
deterministically converted to logical time interval. **/
static __thread timespec my_base_time = {0, 0};

timespec time_diff(const timespec &start, const timespec &end)
{
  timespec tmp;
  if ((end.tv_nsec-start.tv_nsec) < 0) {
    tmp.tv_sec = end.tv_sec - start.tv_sec - 1;
    tmp.tv_nsec = 1000000000 - start.tv_nsec + end.tv_nsec;
  } else {
    tmp.tv_sec = end.tv_sec - start.tv_sec;
    tmp.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return tmp;
}

timespec update_time()
{
  timespec start_time;
  if (options::log_sync) {
    clock_gettime(CLOCK_REALTIME , &start_time);
    timespec ret = time_diff(my_time, start_time);
    my_time = start_time; 
    return ret;
  } else
    return start_time;
}

void check_options()
{
  if (!options::DMT)
    fprintf(stderr, "WARNING: DMT mode is off. The system won't enter scheduler in "
    "LD_PRELOAD mode!!\n");
  if (!options::RR_ignore_rw_regular_file)
    fprintf(stderr, "WARNING: RR_ignore_rw_regular_file is off, and so we can have "
      "non-determinism on regular file I/O!!\n");
}

void InstallRuntime() {
  check_options();
  Runtime::the = new RecorderRT<RRScheduler>;
}

template <typename _S>
int RecorderRT<_S>::syncWait(void *chan, unsigned timeout) {
#ifdef XTERN_PLUS_DBUG
    dprintf("Parrot pid %d, tid %d self %u dbug waiting...\n", getpid(), _S::self(), (unsigned)pthread_self());
  Runtime::__thread_waiting();
#endif
  return _S::wait(chan, timeout);
}

template <typename _S>
void RecorderRT<_S>::syncSignal(void *chan, bool all) {
  std::list<int> signal_list = _S::signal(chan, all);
#ifdef XTERN_PLUS_DBUG
  std::list<int>::iterator itr;
  for (itr = signal_list.begin(); itr != signal_list.end(); itr++) {
    pthread_t tid = _S::getPthreadTid(*itr);
    Runtime::__thread_active(tid);
    dprintf("Parrot pid %d self %u tid %d signals tid %d self %u dbug active\n", 
      getpid(), (unsigned)pthread_self(), _S::self(), *itr, (unsigned)tid);
  }
#endif
}

template <typename _S>
int RecorderRT<_S>::absTimeToTurn(const struct timespec *abstime)
{
  // TODO: convert physical time to logical time (number of turns)
  return _S::getTurnCount() + 30; //rand() % 10;
}

int time2turn(uint64_t nsec)
{
  if (!options::launch_idle_thread) {
    fprintf(stderr, "WARN: converting phyiscal time to logical time \
      without launcing idle thread. Please set 'launch_idle_thread' to 1 and then \
      rerun.\n");
    exit(1);
  }

  const uint64_t MAX_REL = (1000000); // maximum number of turns to wait

  uint64_t ret64 = nsec / options::nanosec_per_turn;

  // if result too large, return MAX_REL
  int ret = (ret64 > MAX_REL) ? MAX_REL : (int) ret64;

  return ret;
}

template <typename _S>
int RecorderRT<_S>::relTimeToTurn(const struct timespec *reltime)
{
  if (!reltime) return 0;

  int ret;
  int64_t ns;

  ns = reltime->tv_sec;
  ns = ns * (1000000000) + reltime->tv_nsec;
  ret = time2turn(ns);

  // if result too small or negative, return only (5 * nthread + 1)
  ret = (ret < 5 * _S::nthread + 1) ? (5 * _S::nthread + 1) : ret;
  dprintf("computed turn = %d\n", ret);
  return ret;
}

template <typename _S>
void RecorderRT<_S>::progBegin(void) {
  Logger::progBegin();
}

template <typename _S>
void RecorderRT<_S>::progEnd(void) {
  Logger::progEnd();
}

/*
 *  This is a fake API function that advances clock when all the threads  
 *  are blocked. 
 */
template <typename _S>
void RecorderRT<_S>::idle_sleep(void) {
  _S::getTurn();
  int turn = _S::incTurnCount();
  assert(turn >= 0);
  timespec ts;
  if (options::log_sync)
    Logger::the->logSync(0, syncfunc::tern_idle, turn, ts, ts, ts, true);
  _S::putTurn();
}

template <typename _S>
void RecorderRT<_S>::idle_cond_wait(void) {
  _S::getTurn();
  int turn = _S::incTurnCount();
  assert(turn >= 0);

  /* Currently idle thread must be in runq since it has grabbed the idle_mutex,
    so >=2 means there is at least one real thread in runq as well. */
  if (_S::runq.size() >= 2)
    _S::idleThreadCondWait();
  else
    _S::putTurn();
}

/*
template <>
void RecorderRT<RecordSerializer>::idle_sleep(void) {
  ::usleep(10);
}
*/

#define BLOCK_TIMER_START(sync_op, ...) \
  if (options::record_runtime_stat) \
    stat.nInterProcSyncOp++; \
  if (options::enforce_non_det_annotations && inNonDet) { \
    return Runtime::__##sync_op(__VA_ARGS__); \
  } \
  if (_S::interProStart()) { \
    _S::block(); \
  } \
  Runtime::__attach_self_to_dbug(__FUNCTION__);
  //fprintf(stderr, "\n\nBLOCK_TIMER_START ins %p, pid %d, self %u, tid %d, turnCount %u, function %s\n", (void *)ins, getpid(), (unsigned)pthread_self(), _S::self(), _S::turnCount, __FUNCTION__);
// At this moment, since self-thread is ahead of the run queue, so this block() should be very fast.
// TBD: do we need logging here? We can, but not sure whether we need to do this.


#define BLOCK_TIMER_END(syncop, ...) \
  Runtime::__detach_self_from_dbug(__FUNCTION__); \
  int backup_errno = errno; \
  if (_S::interProEnd()) { \
    _S::wakeup(); \
  } \
  errno = backup_errno;
  //fprintf(stderr, "\n\nBLOCK_TIMER_END ins %p, pid %d, self %u, tid %d, turnCount %u, function %s\n", (void *)ins, getpid(), (unsigned)pthread_self(), _S::self(), _S::turnCount, __FUNCTION__);

#define SCHED_TIMER_START \
  unsigned nturn; \
  if (options::enforce_non_det_annotations) \
     assert(!inNonDet); \
  timespec app_time = update_time(); \
  record_rdtsc_op("GET_TURN", "START", 2, NULL); \
  _S::getTurn(); \
  record_rdtsc_op("GET_TURN", "END", 2, NULL); \
  if (options::record_runtime_stat && pthread_self() != idle_th) \
     stat.nDetPthreadSyncOp++; \
  timespec sched_time = update_time();
  //if (_S::self() != 1)
    //fprintf(stderr, "\n\nSCHED_TIMER_START ins %p, pid %d, self %u, tid %d, turnCount %u, function %s\n", (void *)ins, getpid(), (unsigned)pthread_self(), _S::self(), _S::turnCount, __FUNCTION__);

#define SCHED_TIMER_END_COMMON(syncop, ...) \
  int backup_errno = errno; \
  timespec syscall_time = update_time(); \
  nturn = _S::incTurnCount(); \
  if (options::log_sync) \
    Logger::the->logSync(ins, (syncop), nturn = _S::getTurnCount(), app_time, syscall_time, sched_time, true, __VA_ARGS__);
   
#define SCHED_TIMER_END(syncop, ...) \
  SCHED_TIMER_END_COMMON(syncop, __VA_ARGS__); \
  _S::putTurn();\
  errno = backup_errno;
  //if (_S::self() != 1)
    //fprintf(stderr, "\n\nSCHED_TIMER_END ins %p, pid %d, self %u, tid %d, turnCount %u, function %s\n", (void *)ins, getpid(), (unsigned)pthread_self(), _S::self(), _S::turnCount, __FUNCTION__);

#define SCHED_TIMER_THREAD_END(syncop, ...) \
  SCHED_TIMER_END_COMMON(syncop, __VA_ARGS__); \
  _S::putTurn(true);\
  errno = backup_errno; 
  
#define SCHED_TIMER_FAKE_END(syncop, ...) \
  nturn = _S::incTurnCount(); \
  timespec fake_time = update_time(); \
  if (options::log_sync) \
    Logger::the->logSync(ins, syncop, nturn, app_time, fake_time, sched_time, /* before */ false, __VA_ARGS__); 

template <typename _S>
void RecorderRT<_S>::printStat(){
  // We must get turn, and print, and then put turn. This is a solid way of 
  // getting deterministic runtime stat.
  _S::getTurn();
  if (options::record_runtime_stat)
    stat.print();
  _S::incTurnCount();
  _S::putTurn();
}
  
template <typename _S>
void RecorderRT<_S>::threadBegin(void) {
  pthread_t th = pthread_self();
  unsigned ins = INVALID_INSID;

  if(_S::self() != _S::MainThreadTid) {
    sem_wait(&thread_begin_sem);
    _S::self(th);
    sem_post(&thread_begin_done_sem);
  }
  assert(_S::self() != _S::InvalidTid);

  SCHED_TIMER_START;
  
  app_time.tv_sec = app_time.tv_nsec = 0;
  Logger::threadBegin(_S::self());

  SCHED_TIMER_END(syncfunc::tern_thread_begin, (uint64_t)th);
}

template <typename _S>
void RecorderRT<_S>::threadEnd(unsigned ins) {
  SCHED_TIMER_START;
  pthread_t th = pthread_self();

  SCHED_TIMER_THREAD_END(syncfunc::tern_thread_end, (uint64_t)th);
  
  Logger::threadEnd();
}

/// The pthread_create wrapper solves three problems.
///
/// Problem 1.  We must assign a logical tern tid to the new thread while
/// holding turn, or multiple newly created thread could get their logical
/// tids nondeterministically.  To do so, we assign a logical tid to a new
/// thread in the thread that creates the new thread.
///
/// If we were to assign this logical id in the new thread itself, we may
/// run into nondeterministic runs, as illustrated by the following
/// example
///
///       t0        t1           t2            t3
///    getTurn();
///    create t2
///    putTurn();
///               getTurn();
///               create t3
///               putTurn();
///                                         getTurn();
///                                         get turn tid 2
///                                         putTurn();
///                            getTurn();
///                            get turn tid 3
///                            putTurn();
///
/// in a different run, t1 may run first and get turn tid 2.
///
/// Problem 2.  When a child thread is created, the child thread may run
/// into a getTurn() before the parent thread has assigned a logical tid
/// to the child thread.  This causes getTurn to refer to self_tid, which
/// is undefined.  To solve this problem, we use @thread_begin_sem to
/// create a thread in suspended mode, until the parent thread has
/// assigned a logical tid for the child thread.
///
/// Problem 3.  We can use one semaphore to create a child thread
/// suspended.  However, when there are two pthread_create() calls, we may
/// still run into cases where a child thread tries to get its
/// thread-local tid but gets -1.  consider
///
///       t0        t1           t2            t3
///    getTurn();
///    create t2
///    sem_post(&thread_begin_sem)
///    putTurn();
///               getTurn();
///               create t3
///                                         sem_wait(&thread_begin_sem);
///                                         self_tid = TidMap[pthread_self()]
///               sem_post(&thread_begin_sem)
///               putTurn();
///                                         getTurn();
///                                         get turn tid 2
///                                         putTurn();
///                            sem_wait(&thread_begin_sem);
///                            self_tid = TidMap[pthread_self()]
///                            getTurn();
///                            get turn tid 3
///                            putTurn();
///
/// The crux of the problem is that multiple sem_post can pair up with
/// multiple sem_down in different ways.  We solve this problem using
/// another semaphore, thread_begin_done_sem.
///
template <typename _S>
int RecorderRT<_S>::pthreadCreate(unsigned ins, int &error, pthread_t *thread,
         pthread_attr_t *attr, void *(*thread_func)(void*), void *arg) {
  int ret;
  SCHED_TIMER_START;

  ret = __tern_pthread_create(thread, attr, thread_func, arg);
  assert(!ret && "failed sync calls are not yet supported!");
  _S::create(*thread);

  SCHED_TIMER_END(syncfunc::pthread_create, (uint64_t)*thread, (uint64_t) ret);
 
  sem_post(&thread_begin_sem);
  sem_wait(&thread_begin_done_sem);
  
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadJoin(unsigned ins, int &error, pthread_t th, void **rv) {
  int ret;

#ifdef XTERN_PLUS_DBUG
  /*
    This is a temp hack, because currently there is conflict between the join 
    of xtern and join of dbug. Xtern actually waits for the child thread to be 
    dead and then proceed, and dbug actually have to pairwise both 
    child-thread-exit and join of parent. This is conflicting.
    So the current hack (xtern gives up the order) is just do not involve xtern but just directly move 
    parent from runq first (because there may be non-det to wait for runq
    to be empty), calls the real join (and jump into dbug), and then call 
    _S::join() to clean the zombies set, and then put myself back to runq.
  */
  if (th != idle_th) {
    //fprintf(stderr, "\n\nxtern::Thread %u calls pthreadJoin start...\n\n\n", (unsigned)pthread_self());
    _S::block();
    ret = Runtime::__pthread_join(th, rv);
    _S::join(th); // This may be dangerous because current thread is not having the turn.
    _S::wakeup();
    //fprintf(stderr, "\n\nxtern::Thread %u calls pthreadJoin end, ok...\n\n\n", (unsigned)pthread_self());
    return ret;
  }
#endif

  SCHED_TIMER_START;
  // NOTE: can actually use if(!_S::zombie(th)) for DMT schedulers because
  // their wait() won't return until some thread signal().
  while(!_S::zombie(th)) {
    /* Don't call syncWait here, but this scheduler wait, because there is a pairwise 
      signal() in putTurn with thread_end. */
    _S::wait((void*)th);
  }
  errno = error;

  ret = pthread_join(th, rv);
  /*if(options::pthread_tryjoin) {
    // FIXME: sometimes a child process gets stuck in
    // pthread_join(idle_th, NULL) in __tern_prog_end(), perhaps because
    // idle_th has become zombie and since the program is exiting, the
    // zombie thread is reaped.
    for(int i=0; i<10; ++i) {
      ret = pthread_tryjoin_np(th, rv);
      if(ret != EBUSY)
        break;
      ::usleep(10);
    }
    if(ret == EBUSY) {
      dprintf("can't join thread; try canceling it instead");
      pthread_cancel(th);
      ret = 0;
    }
  } else {
    ret = pthread_join(th, rv);
  }*/

  error = errno;
  assert(!ret && "failed sync calls are not yet supported!");
  _S::join(th);

  SCHED_TIMER_END(syncfunc::pthread_join, (uint64_t)th);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__pthread_detach(unsigned insid, int &error, pthread_t th) {
  BLOCK_TIMER_START(pthread_detach, insid, error, th);
  int ret = Runtime::__pthread_detach(insid, error, th);
  BLOCK_TIMER_END(syncfunc::pthread_detach, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexInit(unsigned ins, int &error, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
{
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)mutex);
    dprintf("Thread tid %d, self %u is calling non-det pthread_mutex_init.\n", _S::self(), (unsigned)pthread_self());
    return Runtime::__pthread_mutex_init(ins, error, mutex, mutexattr);
  }
  SCHED_TIMER_START;
  errno = error;
  ret = pthread_mutex_init(mutex, mutexattr);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_mutex_init, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexDestroy(unsigned ins, int &error, pthread_mutex_t *mutex)
{
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)mutex);
    return Runtime::__pthread_mutex_destroy(ins, error, mutex);
  }
  SCHED_TIMER_START;
  errno = error;
  ret = pthread_mutex_destroy(mutex);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_mutex_destroy, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexLockHelper(pthread_mutex_t *mu, unsigned timeout) {
  int ret;
  while((ret=pthread_mutex_trylock(mu))) {
    assert(ret==EBUSY && "failed sync calls are not yet supported!");
    ret = syncWait(mu, timeout);
    if(ret == ETIMEDOUT)
      return ETIMEDOUT;
  }
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadRWLockWrLockHelper(pthread_rwlock_t *rwlock, unsigned timeout) {
  int ret;
  while((ret=pthread_rwlock_trywrlock(rwlock))) {
    assert(ret==EBUSY && "failed sync calls are not yet supported!");
    ret = syncWait(rwlock, timeout);
    if(ret == ETIMEDOUT)
      return ETIMEDOUT;
  }
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadRWLockRdLockHelper(pthread_rwlock_t *rwlock, unsigned timeout) {
  int ret;
  while((ret=pthread_rwlock_tryrdlock(rwlock))) {
    assert(ret==EBUSY && "failed sync calls are not yet supported!");
    ret = syncWait(rwlock, timeout);
    if(ret == ETIMEDOUT)
      return ETIMEDOUT;
  }
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexLock(unsigned ins, int &error, pthread_mutex_t *mu) {
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)mu);
    dprintf("Ins %p :   Thread tid %d, self %u is calling non-det pthread_mutex_lock.\n", (void *)ins, _S::self(), (unsigned)pthread_self());
    return Runtime::__pthread_mutex_lock(ins, error, mu);
  }
  SCHED_TIMER_START;
  errno = error;
  pthreadMutexLockHelper(mu);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_mutex_lock, (uint64_t)mu);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::__pthread_rwlock_rdlock(unsigned ins, int &error, pthread_rwlock_t *rwlock)
{
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)rwlock);
    return pthread_rwlock_rdlock(rwlock);
  }
  SCHED_TIMER_START;
  errno = error;
  pthreadRWLockRdLockHelper(rwlock);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_rwlock_rdlock, (uint64_t)rwlock);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::__pthread_rwlock_wrlock(unsigned ins, int &error, pthread_rwlock_t *rwlock)
{
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)rwlock);
    return pthread_rwlock_wrlock(rwlock);
  }
  SCHED_TIMER_START;
  errno = error;
  pthreadRWLockWrLockHelper(rwlock);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_rwlock_wrlock, (uint64_t)rwlock);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::__pthread_rwlock_tryrdlock(unsigned ins, int &error, pthread_rwlock_t *rwlock)
{
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)rwlock);
    return pthread_rwlock_tryrdlock(rwlock);
  }
  SCHED_TIMER_START;
  errno = error;
  int ret = pthread_rwlock_trywrlock(rwlock); //  FIXME now using wrlock for all rdlock
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_rwlock_tryrdlock, (uint64_t)rwlock, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__pthread_rwlock_trywrlock(unsigned ins, int &error, pthread_rwlock_t *rwlock)
{
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)rwlock);
    return pthread_rwlock_trywrlock(rwlock);
  }
  SCHED_TIMER_START;
  errno = error;
  int ret = pthread_rwlock_trywrlock(rwlock); 
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_rwlock_trywrlock, (uint64_t)rwlock, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__pthread_rwlock_unlock(unsigned ins, int &error, pthread_rwlock_t *rwlock)
{
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)rwlock);
    return pthread_rwlock_unlock(rwlock);
  }
  SCHED_TIMER_START;

  errno = error;
  ret = pthread_rwlock_unlock(rwlock);
  error = errno;
  syncSignal(rwlock);
 
  SCHED_TIMER_END(syncfunc::pthread_rwlock_unlock, (uint64_t)rwlock, (uint64_t) ret);

  return ret;
}

template <typename _S>
int RecorderRT<_S>::__pthread_rwlock_destroy(unsigned ins, int &error, pthread_rwlock_t *rwlock) {
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)rwlock);
    return pthread_rwlock_destroy(rwlock);
  }
  SCHED_TIMER_START;
  errno = error;
  int ret = pthread_rwlock_destroy(rwlock); 
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_rwlock_destroy, (uint64_t)rwlock, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__pthread_rwlock_init(unsigned ins, int &error, pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr) {
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)rwlock);
    return pthread_rwlock_init(rwlock, attr);
  }
  SCHED_TIMER_START;
  errno = error;
  int ret = pthread_rwlock_init(rwlock, attr); 
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_rwlock_init, (uint64_t)rwlock, attr, (uint64_t) ret);
  return ret;
}

/// instead of looping to get lock as how we implement the regular lock(),
/// here just trylock once and return.  this preserves the semantics of
/// trylock().
template <typename _S>
int RecorderRT<_S>::pthreadMutexTryLock(unsigned ins, int &error, pthread_mutex_t *mu) {
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)mu);
    return pthread_mutex_trylock(mu);
  }
  SCHED_TIMER_START;
  errno = error;
  ret = pthread_mutex_trylock(mu);
  error = errno;
  assert((!ret || ret==EBUSY)
         && "failed sync calls are not yet supported!");
  SCHED_TIMER_END(syncfunc::pthread_mutex_trylock, (uint64_t)mu, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexTimedLock(unsigned ins, int &error, pthread_mutex_t *mu,
                                                const struct timespec *abstime) {
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)mu);
    return pthread_mutex_timedlock(mu, abstime);
  }
  if(abstime == NULL)
    return pthreadMutexLock(ins, error, mu);

  timespec cur_time, rel_time;
  if (my_base_time.tv_sec == 0) {
    fprintf(stderr, "WARN: pthread_mutex_timedlock has a non-det timeout. \
    Please use it with tern_set_base_timespec().\n");
    clock_gettime(CLOCK_REALTIME, &cur_time);
  } else {
    cur_time.tv_sec = my_base_time.tv_sec;
    cur_time.tv_nsec = my_base_time.tv_nsec;
  }
  rel_time = time_diff(cur_time, *abstime);

  SCHED_TIMER_START;
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(&rel_time);
  errno = error;
  int ret = pthreadMutexLockHelper(mu, timeout);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_mutex_timedlock, (uint64_t)mu, (uint64_t) ret);
 
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexUnlock(unsigned ins, int &error, pthread_mutex_t *mu){
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)mu);
    dprintf("Thread tid %d, self %u is calling non-det pthread_mutex_unlock.\n", _S::self(), (unsigned)pthread_self());
    return Runtime::__pthread_mutex_unlock(ins, error, mu);
  }
  //fprintf(stderr, "pthreadMutexUnlock1\n");
  SCHED_TIMER_START;
  //fprintf(stderr, "pthreadMutexUnlock2\n");
  errno = error;
  ret = pthread_mutex_unlock(mu);
  error = errno;
  //fprintf(stderr, "pthreadMutexUnlock3\n");
  assert(!ret && "failed sync calls are not yet supported!");
  syncSignal(mu);
  //fprintf(stderr, "pthreadMutexUnlock4\n");
  SCHED_TIMER_END(syncfunc::pthread_mutex_unlock, (uint64_t)mu, (uint64_t) ret);

  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadBarrierInit(unsigned ins, int &error, pthread_barrier_t *barrier,
                                       unsigned count) {
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)barrier);
    return pthread_barrier_init(barrier, NULL, count);
  }
  SCHED_TIMER_START;
  errno = error;
  ret = pthread_barrier_init(barrier, NULL, count);
  error = errno;
  assert(!ret && "failed sync calls are not yet supported!");
  assert(barriers.find(barrier) == barriers.end()
         && "barrier already initialized!");
  barriers[barrier].count = count;
  barriers[barrier].narrived = 0;

  SCHED_TIMER_END(syncfunc::pthread_barrier_init, (uint64_t)barrier, (uint64_t) count);
 
  return ret;
}

/// barrier_wait has a similar problem to pthread_cond_wait (see below).
/// we want to avoid the head of queue block, so must call syncWait(ba) and
/// give up turn before calling pthread_barrier_wait.  However, when the
/// last thread arrives, we must wake up the waiting threads.
///
/// solution: we keep track of the barrier count by ourselves, so that the
/// last thread arriving at the barrier can figure out that it is the last
/// thread, and wakes up all other threads.
///
template <typename _S>
int RecorderRT<_S>::pthreadBarrierWait(unsigned ins, int &error, 
                                       pthread_barrier_t *barrier) {
  /// Note: the syncSignal() operation has to be done while the thread has the
  /// turn; otherwise two independent syncSignal() operations on two
  /// independent barriers can be nondeterministic.  e.g., suppose two
  /// barriers ba1 and ba2 each has count 1.
  ///
  ///       t0                        t1
  ///
  /// getTurn()
  /// syncWait(ba1);
  ///                         getTurn()
  ///                         syncWait(ba1);
  /// barrier_wait(ba1)
  ///                         barrier_wait(ba2)
  /// syncSignal(ba1)             syncSignal(ba2)
  ///
  /// these two syncSignal() ops can be nondeterministic, causing threads to be
  /// added to the activeq in different orders
  ///
  
    
  
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)barrier);
    return pthread_barrier_wait(barrier);
  }
  SCHED_TIMER_START;
  SCHED_TIMER_FAKE_END(syncfunc::pthread_barrier_wait, (uint64_t)barrier);
  
  barrier_map::iterator bi = barriers.find(barrier);
  assert(bi!=barriers.end() && "barrier is not initialized!");
  barrier_t &b = bi->second;

  ++ b.narrived;

//#define xxx
#ifdef xxx
  fprintf(stderr, "thread %d arrives at barrier, b.count = %d, b.arrvied = %d\n", 
    _S::self(), (int) b.count, (int) b.narrived);
  fflush(stderr);
#endif

  assert(b.narrived <= b.count && "barrier overflow!");
  if(b.count == b.narrived) {
    b.narrived = 0; // barrier may be reused
    syncSignal(barrier, /*all=*/true);
    // according to the man page of pthread_barrier_wait, one of the
    // waiters should return PTHREAD_BARRIER_SERIAL_THREAD, instead of 0
    ret = PTHREAD_BARRIER_SERIAL_THREAD;
#ifdef xxx
    fprintf(stderr, "thread %d claims itself as the last one, b.count = %d, b.arrvied = %d\n", 
      _S::self(), (int) b.count, (int) b.narrived);
    fflush(stderr);
#endif

    _S::putTurn();  // this gives _first and _second different turn numbers.
    _S::getTurn();
  } else {
    ret = 0;
    syncWait(barrier);
  }
  sched_time = update_time();
#ifdef xxx
  fprintf(stderr, "thread %d leaves barrier\n", _S::self());
  fflush(stderr);
#endif

  SCHED_TIMER_END(syncfunc::pthread_barrier_wait, (uint64_t)barrier);

  return ret;
}

// FIXME: the handling of the EBUSY case seems gross
template <typename _S>
int RecorderRT<_S>::pthreadBarrierDestroy(unsigned ins, int &error, 
                                          pthread_barrier_t *barrier) {
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)barrier);
    return pthread_barrier_destroy(barrier);
  }
  SCHED_TIMER_START;
  errno = error;
  ret = pthread_barrier_destroy(barrier);
  error = errno;

  // pthread_barrier_destroy returns EBUSY if the barrier is still in use
  assert((!ret || ret==EBUSY) && "failed sync calls are not yet supported!");
  if(!ret) {
    barrier_map::iterator bi = barriers.find(barrier);
    assert(bi != barriers.end() && "barrier not initialized!");
    barriers.erase(bi);
  }
  
  SCHED_TIMER_END(syncfunc::pthread_barrier_destroy, (uint64_t)barrier, (uint64_t) ret);
   
  return ret;
}

/// The issues with pthread_cond_wait()
///
/// ------ First issue: deadlock. Normally, we'd want to do
///
///   getTurn();
///   pthread_cond_wait(&cv, &mu);
///   putTurn();
///
/// However, pthread_cond_wait blocks, so we won't call putTurn(), thus
/// deadlocking the entire system.  And there is (should be) no
/// cond_trywait, unlike in the case of pthread_mutex_t.
///
///
/// ------ A naive solution is to putTurn before calling pthread_cond_wait
///
///   getTurn();
///   putTurn();
///   pthread_cond_wait(&cv, &mu);
///
/// However, this is nondeterministic.  Two nondeterminism scenarios:
///
/// 1: race with a pthread_mutex_lock() (or trylock()).  Depending on the
/// timing, the lock() may or may not succeed.
///
///   getTurn();
///   putTurn();
///                                     getTurn();
///   pthread_cond_wait(&cv, &mu);      pthread_mutex_lock(&mu) (or trylock())
///                                     putTurn();
///
/// 2: race with pthread_cond_signal() (or broadcast()).  Depending on the
/// timing, the signal may or may not be received.  Note the code below uses
/// "naked signal" without holding the lock, which is wrong, but despite so,
/// we still have to be deterministic.
///
///   getTurn();
///   putTurn();
///                                      getTurn();
///   pthread_cond_wait(&cv, &mu);       pthread_cond_signal(&cv) (or broadcast())
///                                      putTurn();
///
///
/// ------ First solution: replace mu with the scheduler mutex
///
///   getTurn();
///   pthread_mutex_unlock(&mu);
///   syncSignal(&mu);
///   waitFirstHalf(&cv); // put self() to waitq for &cv, but don't wait on
///                       // any internal sync obj or release scheduler lock
///                       // because we'll wait on the cond var in user
///                       // program
///   pthread_cond_wait(&cv, &schedulerLock);
///   getTurnNU(); // get turn without lock(&schedulerLock), but
///                // unlock(&schedulerLock) when returns
///
///   pthreadMutexLockHelper(mu); // call lock wrapper
///   putTurn();
///
/// This solution solves race 1 & 2 at the same time because getTurn has to
/// wait until schedulerLock is released.
///
///
/// ------ Issue with the first solution: deadlock.
///
/// The first solution may still deadlock because the thread we
/// deterministically choose to wake up may be different from the one
/// chosen nondeterministically by pthread.
///
/// consider:
///
///   pthread_cond_wait(&cv, &schedulerLock);      pthread_cond_wait(&cv, &schedulerLock);
///   getTurnWithoutLock();                        getTurnWithoutLock()
///   ret = pthread_mutex_trylock(&mu);            ret = pthread_mutex_trylock(&mu);
///   ...                                          ...
///   putTurn();                                   putTurn();
///
/// When a thread calls pthread_cond_signal, suppose pthread chooses to
/// wake up the second thread.  However, the first thread may be the one
/// that gets the turn first.  Therefore the system would deadlock.
///
///
/// ------- Second solution: replace pthread_cond_signal with
/// pthread_cond_broadcast, which wakes up all threads, and then the
/// thread that gets the turn first would proceed first to get mu.
///
/// pthread_cond_signal(cv):
///
///   getTurnLN();  // lock(&schedulerLock) but does not unlock(&schedulerLock)
///   pthread_cond_broadcast(cv);
///   signalNN(cv); // wake up the first thread waiting on cv; need to hold
///                 // schedulerLock because signalNN touches wait queue
///   putTurnNU();  // no lock() but unlock(&schedulerLock)
///
/// This is the solution currently implement.
///
///
/// ------- Issues with the second solution: changed pthread_cond_signal semantics
///
/// Once we change cond_signal to cond_broadcast, all woken up threads can
/// proceed after the first thread releases mu.  This differs from the
/// cond var semantics where one signal should only wake up one thread, as
/// seen in the Linux man page:
///
///   !pthread_cond_signal! restarts one of the threads that are waiting
///   on the condition variable |cond|. If no threads are waiting on
///   |cond|, nothing happens. If several threads are waiting on |cond|,
///   exactly one is restarted, but it is not specified which.
///
/// If the application program correctly uses mesa-style cond var, i.e.,
/// the woken up thread re-checks the condition using while, waking up
/// more than one thread would not be an issue.  In addition, according
/// to the IEEE/The Open Group, 2003, PTHREAD_COND_BROADCAST(P):
///
///   In a multi-processor, it may be impossible for an implementation of
///   pthread_cond_signal() to avoid the unblocking of more than one
///   thread blocked on a condition variable."
///
/// However, we have to be deterministic despite program errors (again).
///
///
/// ------- Third (proposed, not yet implemented) solution: replace cv
///         with our own cv in the actual pthread_cond_wait
///
/// pthread_cond_wait(cv, mu):
///
///   getTurn();
///   pthread_mutex_unlock(&mu);
///   syncSignal(&mu);
///   waitFirstHalf(&cv);
///   putTurnLN();
///   pthread_cond_wait(&waitcv[self()], &schedulerLock);
///   getTurnNU();
///   pthreadMutexLockHelper(mu);
///   putTurn();
///
/// pthread_cond_signal(&cv):
///
///   getTurnLN();
///   signalNN(&cv) // wake up precisely the first thread with waitVar == chan
///   pthread_cond_signal(&cv); // no op
///   putTurnNU();
///
/// ----- Fourth (proposed, not yet implemented) solution: re-implement
///       pthread cv all together
///
/// A closer look at the code shows that we're not really using the
/// original conditional variable at all.  That is, no thread ever waits
/// on the original cond_var (ever calls cond_wait() on the cond var).  We
/// may as well skip cond_signal.  That is, we're reimplementing cond vars
/// with our own queues, lock and cond vars.
///
/// This observation motives our next design: re-implementing cond var on
/// top of semaphore, so that we can get rid of the schedulerLock.  We can
/// implement the scheduler functions as follows:
///
/// getTurn():
///   sem_wait(&semOfThread);
///
/// putTurn():
///   move self to end of active q
///   find sem of head of q
///   sem_post(&semOfHead);
///
/// wait():
///   move self to end of wait q
///   find sem of head of q
///   sem_post(&sem_of_head);
///
/// We can then implement pthread_cond_wait and pthread_cond_signal as follows:
///
/// pthread_cond_wait(&cv, &mu):
///   getTurn();
///   pthread_mutex_unlock(&mu);
///   syncSignal(&mu);
///   wait(&cv);
///   putTurn();
///
///   getTurn();
///   pthreadMutexLockHelper(&mu);
///   putTurn();
///
/// pthread_cond_signal(&cv):
///   getTurn();
///   syncSignal(&cv);
///   putTurn();
///
/// One additional optimization we can do for programs that do sync
/// frequently (1000 ops > 1 second?): replace the semaphores with
/// integer flags.
///
///   sem_wait(semOfThread) ==>  while(flagOfThread != 1);
///   sem_post(semOfHead)   ==>  flagOfHead = 1;
///
///
///  ----- Fifth (proposed, probably not worth implementing) solution: can
///        be more aggressive and implement more stuff on our own (mutex,
///        barrier, etc), but probably unnecessary.  skip sleep and
///        barrier_wait help the most
///
///
///  ---- Summary
///
///  solution 2: lock + cv + cond_broadcast.  deterministic if application
///  code use while() with cond_wait, same sync var state except short
///  periods of time within cond_wait.  the advantage is that it ensures
///  that all sync vars have the same internal states, in case the
///  application code breaks abstraction boundaries and reads the internal
///  states of the sync vars.
///
///  solution 3: lock + cv + replace cv.  deterministic, but probably not
///  as good as semaphore since the schedulerLock seems unnecessary.  the
///  advantage is that it ensures that all sync vars except original cond
///  vars have the same internal states
///
///  solution 4: semaphore.  deterministic, good if sync frequency
///  low.
///
///  solution 4 optimization: flag.  deterministic, good if sync
///  frequency high
///
///  solution 5: probably not worth it
///
template <typename _S>
int RecorderRT<_S>::pthreadCondWait(unsigned ins, int &error, 
                                    pthread_cond_t *cv, pthread_mutex_t *mu){
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)cv);
    add_non_det_var((void *)mu);
    return pthread_cond_wait(cv, mu);
  }
  SCHED_TIMER_START;
  pthread_mutex_unlock(mu);
  syncSignal(mu);

  SCHED_TIMER_FAKE_END(syncfunc::pthread_cond_wait, (uint64_t)cv, (uint64_t)mu);
  syncWait(cv);
  sched_time = update_time();
  errno = error;
  pthreadMutexLockHelper(mu);
  error = errno;
  
  SCHED_TIMER_END(syncfunc::pthread_cond_wait, (uint64_t)cv, (uint64_t)mu);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondTimedWait(unsigned ins, int &error, 
    pthread_cond_t *cv, pthread_mutex_t *mu, const struct timespec *abstime){

  int saved_ret = 0;

  if(abstime == NULL)
    return pthreadCondWait(ins, error, cv, mu);

  timespec cur_time, rel_time;
  if (my_base_time.tv_sec == 0) {
    fprintf(stderr, "WARN: pthread_cond_timedwait has a non-det timeout. \
    Please add tern_set_base_timespec().\n");
    clock_gettime(CLOCK_REALTIME, &cur_time);
  } else {
    cur_time.tv_sec = my_base_time.tv_sec;
    cur_time.tv_nsec = my_base_time.tv_nsec;
  }
  rel_time = time_diff(cur_time, *abstime);

  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)cv);
    add_non_det_var((void *)mu);
    return pthread_cond_timedwait(cv, mu, abstime);
  }
  SCHED_TIMER_START;
  pthread_mutex_unlock(mu);

  SCHED_TIMER_FAKE_END(syncfunc::pthread_cond_timedwait, (uint64_t)cv, (uint64_t)mu, (uint64_t) 0);

  syncSignal(mu);
  unsigned nTurns = relTimeToTurn(&rel_time);
  dprintf("Tid %d pthreadCondTimedWait physical time interval %ld.%ld, logical turns %u\n",
    _S::self(), (long)rel_time.tv_sec, (long)rel_time.tv_nsec, nTurns);
  unsigned timeout = _S::getTurnCount() + nTurns;
  saved_ret = ret = syncWait(cv, timeout);
  dprintf("timedwait return = %d, after %d turns\n", ret, _S::getTurnCount() - nturn);

  sched_time = update_time();
  errno = error;
  pthreadMutexLockHelper(mu);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_cond_timedwait, (uint64_t)cv, (uint64_t)mu, (uint64_t) saved_ret);

  return saved_ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondSignal(unsigned ins, int &error, pthread_cond_t *cv){
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)cv);
    return pthread_cond_signal(cv);
  }
  //fprintf(stderr, "pthreadCondSignal start...\n");
  SCHED_TIMER_START;
  //fprintf(stderr, "pthreadCondSignal start got turn...\n");
  syncSignal(cv);
  //fprintf(stderr, "pthreadCondSignal start got turn2...\n");
  SCHED_TIMER_END(syncfunc::pthread_cond_signal, (uint64_t)cv);
  //fprintf(stderr, "pthreadCondSignal start put turn...\n");
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondBroadcast(unsigned ins, int &error, pthread_cond_t*cv){
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)cv);
    return pthread_cond_broadcast(cv);
  }
  SCHED_TIMER_START;
  syncSignal(cv, /*all=*/true);
  SCHED_TIMER_END(syncfunc::pthread_cond_broadcast, (uint64_t)cv);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::semWait(unsigned ins, int &error, sem_t *sem) {
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)sem);
    //fprintf(stderr, "non det sem wait...\n");
    return Runtime::__sem_wait(ins, error, sem);
  }
  SCHED_TIMER_START;
  while((ret=sem_trywait(sem)) != 0) {
    // WTH? pthread_mutex_trylock returns EBUSY if lock is held, yet
    // sem_trywait returns -1 and sets errno to EAGAIN if semaphore is not
    // available
    assert(errno==EAGAIN && "failed sync calls are not yet supported!");
    syncWait(sem);
  }
  SCHED_TIMER_END(syncfunc::sem_wait, (uint64_t)sem);

  return 0;
}

template <typename _S>
int RecorderRT<_S>::semTryWait(unsigned ins, int &error, sem_t *sem) {
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)sem);
    return sem_trywait(sem);
  }
  SCHED_TIMER_START;
  errno = error;
  ret = sem_trywait(sem);
  error = errno;
  if(ret != 0)
    assert(errno==EAGAIN && "failed sync calls are not yet supported!");
  SCHED_TIMER_END(syncfunc::sem_trywait, (uint64_t)sem, (uint64_t)ret);
 
  return ret;
}

template <typename _S>
int RecorderRT<_S>::semTimedWait(unsigned ins, int &error, sem_t *sem,
                                     const struct timespec *abstime) {
  int saved_err = 0;
  if(abstime == NULL)
    return semWait(ins, error, sem);

  timespec cur_time, rel_time;
  if (my_base_time.tv_sec == 0) {
    fprintf(stderr, "WARN: sem_timedwait has a non-det timeout. \
    Please add tern_set_base_timespec().\n");
    clock_gettime(CLOCK_REALTIME, &cur_time);
  } else {
    cur_time.tv_sec = my_base_time.tv_sec;
    cur_time.tv_nsec = my_base_time.tv_nsec;
  }
  rel_time = time_diff(cur_time, *abstime);
  
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)sem);
    return sem_timedwait(sem, abstime);
  }
  SCHED_TIMER_START;
  
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(&rel_time);
  while((ret=sem_trywait(sem))) {
    assert(errno==EAGAIN && "failed sync calls are not yet supported!");
    ret = syncWait(sem, timeout);
    if(ret == ETIMEDOUT) {
      ret = -1;
      saved_err = ETIMEDOUT;
      error = ETIMEDOUT;
      break;
    }
  }
  SCHED_TIMER_END(syncfunc::sem_timedwait, (uint64_t)sem, (uint64_t)ret);

  errno = saved_err;
  return ret;
}

template <typename _S>
int RecorderRT<_S>::semPost(unsigned ins, int &error, sem_t *sem){
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)sem);
    return Runtime::__sem_post(ins, error, sem);
  }
  SCHED_TIMER_START;
  ret = sem_post(sem);
  assert(!ret && "failed sync calls are not yet supported!");
  syncSignal(sem);
  SCHED_TIMER_END(syncfunc::sem_post, (uint64_t)sem, (uint64_t)ret);
 
  return 0;
}

template <typename _S>
int RecorderRT<_S>::semInit(unsigned ins, int &error, sem_t *sem, int pshared, unsigned int value){
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)sem);
    return Runtime::__sem_init(ins, error, sem, pshared, value);
  }
  SCHED_TIMER_START;
  ret = sem_init(sem, pshared, value);
  assert(!ret && "failed sync calls are not yet supported!");
  SCHED_TIMER_END(syncfunc::sem_init, (uint64_t)sem, (uint64_t)ret);

  return 0;
}

template <typename _S>
void RecorderRT<_S>::lineupInit(long opaque_type, unsigned count, unsigned timeout_turns) {
  unsigned ins = opaque_type;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)opaque_type);
    return;
  }
  SCHED_TIMER_START;
  //fprintf(stderr, "lineupInit opaque_type %p, count %u, timeout %u\n", (void *)opaque_type, count, timeout_turns);
  if (refcnt_bars.find(opaque_type) != refcnt_bars.end()) {
    fprintf(stderr, "refcnt barrier %p already initialized!\n", (void *)opaque_type);
    assert(false);
  }
  refcnt_bars[opaque_type].count = count;
  refcnt_bars[opaque_type].nactive = 0;
  refcnt_bars[opaque_type].timeout = timeout_turns;
  refcnt_bars[opaque_type].setArriving();
  SCHED_TIMER_END(syncfunc::tern_lineup_init, (uint64_t)opaque_type, (uint64_t) count, (uint64_t) timeout_turns);
}

template <typename _S>
void RecorderRT<_S>::lineupDestroy(long opaque_type) {
  unsigned ins = opaque_type;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)opaque_type);
    return;
  }
  SCHED_TIMER_START;
  //fprintf(stderr, "lineupDestroy opaque_type %p\n", (void *)opaque_type);
  assert(refcnt_bars.find(opaque_type) != refcnt_bars.end() && "refcnt barrier is not initialized!");
  refcnt_bars[opaque_type].count = 0;
  refcnt_bars[opaque_type].nactive = 0;
  refcnt_bars[opaque_type].timeout = 0;
  refcnt_bars[opaque_type].nSuccess = 0;
  refcnt_bars[opaque_type].nTimeout = 0;
  refcnt_bars[opaque_type].setArriving();  
  refcnt_bars.erase(opaque_type);
  SCHED_TIMER_END(syncfunc::tern_lineup_destroy, (uint64_t)opaque_type);
}

template <typename _S>
void RecorderRT<_S>::lineupStart(long opaque_type) {
  unsigned ins = opaque_type;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)opaque_type);
    return;
  }
  //fprintf(stderr, "lineupStart opaque_type %p, tid %d, waiting for turn...\n", (void *)opaque_type, _S::self());

  record_rdtsc_op(__FUNCTION__, "START", 1, NULL); // Record rdtsc start, disabled by default.

  SCHED_TIMER_START;
  refcnt_bar_map::iterator bi = refcnt_bars.find(opaque_type);
  assert(bi != refcnt_bars.end() && "refcnt barrier is not initialized!");
  ref_cnt_barrier_t &b = bi->second;
  b.nactive++;  
  //fprintf(stderr, "lineupStart opaque_type %p, tid %d, count %d, nactive %u\n", (void *)opaque_type, _S::self(), b.count, b.nactive);

  if (b.nactive == b.count) {
    if (b.isArriving()) {
      // full, do not reset "nactive", since we are ref-counting barrier..
      /*b.nSuccess++;
      if (b.nSuccess%1000 == 0)
        fprintf(stderr, "lineupStart opaque_type %p, tid %d, full and success (%ld:%ld)!\n",
          (void *)opaque_type, _S::self(), b.nSuccess, b.nTimeout);*/
      if (options::record_runtime_stat)
        stat.nLineupSucc++;
      b.setLeaving();
      syncSignal(&b, true); // Signal all threads blocking on this barrier.
    } else {
      // NOP. There could be a case that after timeout happens,
      // all threads arrive, then we just let them do NOP, and deterministic.
    } 
  } else {
    if (b.isArriving()) {
      syncWait(&b, _S::getTurnCount() + b.timeout);
     
      // Handle timeout here, since the wait() would call getTurn and still grab the turn.
      if (b.nactive < b.count && b.isArriving()) {
        /*b.nTimeout++;
        fprintf(stderr, "lineupStart opaque_type %p, tid %d, timeout  (%ld:%ld)!\n",
          (void *)opaque_type, _S::self(), b.nSuccess, b.nTimeout);*/
        if (options::record_runtime_stat)
          stat.nLineupTimeout++;
        b.setLeaving();
        syncSignal(&b, true); // Signal all threads blocking on this barrier.
      }
    } else {
      // proceed. NOP.
    }
  }
   
  SCHED_TIMER_END(syncfunc::tern_lineup_start, (uint64_t)opaque_type);

  record_rdtsc_op(__FUNCTION__, "END", 1, NULL); // Record rdtsc start, disabled by default.
}

template <typename _S>
void RecorderRT<_S>::lineupEnd(long opaque_type) {
  unsigned ins = opaque_type;
  if (options::enforce_non_det_annotations && inNonDet) {
    if (options::record_runtime_stat)
      stat.nNonDetPthreadSync++;
    add_non_det_var((void *)opaque_type);
    return;
  }
  SCHED_TIMER_START;
  refcnt_bar_map::iterator bi = refcnt_bars.find(opaque_type);
  assert(bi != refcnt_bars.end() && "refcnt barrier is not initialized!");
  ref_cnt_barrier_t &b = bi->second;
  b.nactive--;
  //fprintf(stderr, "lineupEnd opaque_type %p, tid %d, nactive %u\n", (void *)opaque_type, _S::self(), b.nactive);
  if (b.nactive == 0 && b.isLeaving()) {
    b.setArriving();
  }
  SCHED_TIMER_END(syncfunc::tern_lineup_end, (uint64_t)opaque_type);
}

template <typename _S>
void RecorderRT<_S>::nonDetStart() {
  unsigned ins = 0;
  dprintf("nonDetStart, tid %d, self %u\n", _S::self(), (unsigned)pthread_self());
  SCHED_TIMER_START;
  if (options::record_runtime_stat)
    stat.nNonDetRegions++;

  nNonDetWait++;
  /** Although at this moment current thread is still in the xtern runq, we pre-attach it to dbug,
  so that after _S::block() below is called, for whatever operation current thread is going to call,                                    dbug will know totally how many threads 
  should be blocked (so that dbug can explore the upper bound of non-determinism for these
  non-det regions). **/
#ifdef XTERN_PLUS_DBUG
  Runtime::__attach_self_to_dbug(__FUNCTION__);
#endif

  /** All non-det operations are blocked on this fake var until runq is empty, 
  i.e., all valid (except idle thread) xtern threads are paused.
  This wait works like a lineup with unlimited timeout, which is for 
  maximizing the non-det regions. **/
  /* Do not call the wait() here, but _S::wait(), because we do not want to 
  involved dbug_thread_waiting/active here. */
  _S::wait(&nonDetCV);

  nNonDetWait--;

  SCHED_TIMER_END(syncfunc::tern_non_det_start, 0);
  /** Reuse existing xtern API. Get turn, remove myself from runq, and then pass turn. This 
  operation is determinisitc since we get turn. **/
  _S::block();
  dprintf("nonDetStart is done, tid %d, self %u, turnCount %u\n", _S::self(), (unsigned)pthread_self(), _S::turnCount);
  assert(!inNonDet);
  inNonDet = true;
}

template <typename _S>
void RecorderRT<_S>::nonDetEnd() {
  dprintf("nonDetEnd, tid %d, self %u\n", _S::self(), (unsigned)pthread_self());
  assert(options::enforce_non_det_annotations == 1);
  assert(inNonDet);
  inNonDet = false;
  /** At this moment current thread won't call any non-det sync op any more, so we 
  do not need to worry about the order between this non_det_end() and other non-det sync
  in other threads' non-det regions, so we do not need to call the wait(NON_DET_BLOCKED)
  as in non_det_start(). **/
#ifdef XTERN_PLUS_DBUG
  Runtime::__detach_self_from_dbug(__FUNCTION__);
#endif

  _S::wakeup(); /** Reuse existing xtern API. Put myself to wake-up queue, 
                            other threads grabbing the turn will put myself back to runq. This operation is non-
                            determinisit since we do not get turn, but it is fine, because there is already 
                            some non-det sync ops within the region already. Note that after this point, the 
                            status of the thread is still runnable. **/
}

template <typename _S>
void RecorderRT<_S>::threadDetach() {
#ifdef XTERN_PLUS_DBUG
  Runtime::__thread_detach();
#endif
}

template <typename _S>
void RecorderRT<_S>::nonDetBarrierEnd(int bar_id, int cnt) {
  dprintf("nonDetBarrierEnd, tid %d, self %u\n", _S::self(), (unsigned)pthread_self());
  assert(options::enforce_non_det_annotations == 1);
  assert(inNonDet);
  inNonDet = false;
  /** At this moment current thread won't call any non-det sync op any more, so we
  do not need to worry about the order between this non_det_end() and other non-det sync
  in other threads' non-det regions, so we do not need to call the wait(NON_DET_BLOCKED)
  as in non_det_start(). **/
#ifdef XTERN_PLUS_DBUG
  Runtime::__detach_barrier_end(bar_id, cnt);
#endif

  _S::wakeup(); /** Reuse existing xtern API. Put myself to wake-up queue,
                            other threads grabbing the turn will put myself back to runq. This operation is non-
                            determinisit since we do not get turn, but it is fine, because there is already
                            some non-det sync ops within the region already. Note that after this point, the
                            status of the thread is still runnable. **/
}

template <typename _S>
void RecorderRT<_S>::setBaseTime(struct timespec *ts) {
  // Do not need to enforce any turn here.
  dprintf("setBaseTime, tid %d, base time %ld.%ld\n", _S::self(), (long)ts->tv_sec, (long)ts->tv_nsec);
  assert(ts);
  my_base_time.tv_sec = ts->tv_sec;
  my_base_time.tv_nsec = ts->tv_nsec;
}

template <typename _S>
void RecorderRT<_S>::symbolic(unsigned ins, int &error, void *addr,
                              int nbyte, const char *name){
  SCHED_TIMER_START;
  SCHED_TIMER_END(syncfunc::tern_symbolic, (uint64_t)addr, (uint64_t)nbyte);
}

//////////////////////////////////////////////////////////////////////////
// Partially specialize RecorderRT for scheduler RecordSerializer.  The
// RecordSerializer doesn't really care about the order of synchronization
// operations, as long as the log faithfully records the actual order that
// occurs.  Thus, we can simplify the implementation of pthread cond var
// methods for RecordSerializer.

/*
template <>
int RecorderRT<RecordSerializer>::wait(void *chan, unsigned timeout) {
  typedef RecordSerializer _S;
  _S::putTurn();
  sched_yield();
  _S::getTurn();
  return 0;
}

template <>
void RecorderRT<RecordSerializer>::signal(void *chan, bool all) {
}

template <>
int RecorderRT<RecordSerializer>::pthreadMutexTimedLock(unsigned ins, int &error, pthread_mutex_t *mu,
                                                        const struct timespec *abstime) {
  typedef RecordSerializer _S;

  if(abstime == NULL)
    return pthreadMutexLock(ins, error, mu);

  int ret;
  SCHED_TIMER_START;
  while((ret=pthread_mutex_trylock(mu))) {
    assert(ret==EBUSY && "failed sync calls are not yet supported!");

    wait(mu);

    struct timespec curtime;
    struct timeval curtimetv;
    gettimeofday(&curtimetv, NULL);
    curtime.tv_sec = curtimetv.tv_sec;
    curtime.tv_nsec = curtimetv.tv_usec * 1000;
    if(curtime.tv_sec > abstime->tv_sec
       || (curtime.tv_sec == abstime->tv_sec &&
           curtime.tv_nsec >= abstime->tv_nsec)) {
      ret = ETIMEDOUT;
      break;
    }
  }
  SCHED_TIMER_END(syncfunc::pthread_mutex_timedlock, (uint64_t)mu, (uint64_t)ret);
 
  return ret;
}

template <>
int RecorderRT<RecordSerializer>::semTimedWait(unsigned ins, int &error, sem_t *sem,
                                               const struct timespec *abstime) {
  typedef RecordSerializer _S;
  int saved_err = 0;

  if(abstime == NULL)
    return semWait(ins, error, sem);
  
  int ret;
  SCHED_TIMER_START;
  while((ret=sem_trywait(sem))) {
    assert(errno==EAGAIN && "failed sync calls are not yet supported!");
    //_S::putTurn();
    //sched_yield();
    //_S::getTurn();
    wait(sem);

    struct timespec curtime;
    struct timeval curtimetv;
    gettimeofday(&curtimetv, NULL);
    curtime.tv_sec = curtimetv.tv_sec;
    curtime.tv_nsec = curtimetv.tv_usec * 1000;
    if(curtime.tv_sec > abstime->tv_sec
       || (curtime.tv_sec == abstime->tv_sec &&
           curtime.tv_nsec >= abstime->tv_nsec)) {
      ret = -1;
      error = ETIMEDOUT;
      saved_err = ETIMEDOUT;
      break;
    }
  }
  SCHED_TIMER_END(syncfunc::sem_timedwait, (uint64_t)sem, (uint64_t)ret);
  
  errno = saved_err;
  return ret;
}
*/
/// NOTE: recording may be nondeterministic because the order of turns may
/// not be the order in which threads arrive or leave barrier_wait().  if
/// we have N concurrent barrier_wait() but the barrier count is smaller
/// than N, a thread may block in one run but return in another.  This
/// means, in replay, we should not call barrier_wait at all
template <>
int RecorderRT<RecordSerializer>::pthreadBarrierWait(unsigned ins, int &error, 
                                                     pthread_barrier_t *barrier) {
  typedef RecordSerializer _S;
  int ret = 0;
    
  SCHED_TIMER_START;
  SCHED_TIMER_FAKE_END(syncfunc::pthread_barrier_wait, (uint64_t)barrier, (uint64_t)ret);

  _S::putTurn();
  _S::getTurn();  //  more getTurn for consistent number of getTurn with RRSchedler
  _S::incTurnCount();
  _S::putTurn();

  errno = error;
  ret = pthread_barrier_wait(barrier);
  error = errno;
  assert((!ret || ret==PTHREAD_BARRIER_SERIAL_THREAD)
         && "failed sync calls are not yet supported!");

  //timespec app_time = update_time();
  _S::getTurn();
  sched_time = update_time();
  SCHED_TIMER_END(syncfunc::pthread_barrier_wait, (uint64_t)barrier, (uint64_t)ret);

  return ret;
}

/// FCFS version of pthread_cond_wait is a lot simpler than RR version.
///
/// can use lock + cv, and since we don't force threads to get turns in
/// some order, getTurnWithoutLock() is just a noop, and we don't need to
/// replace cond_signal with cond_broadcast.
///
/// semaphore and flag approaches wouldn't make much sense.
///
/// TODO: can we use spinlock instead of schedulerLock?  probably not due
/// to cond_wait problem
///

/// Why do we use our own lock?  To avoid nondeterminism in logging
///
///   getTurn();
///   putTurn();
///   (1) pthread_cond_wait(&cv, &mu);
///                                     getTurn();
///                                     pthread_mutex_lock(&mu) (or trylock())
///                                     putTurn();
///                                     getTurn();
///                                     pthread_signal(&cv)
///                                     putTurn();
///   (2) pthread_cond_wait(&cv, &mu);
///

template <>
int RecorderRT<RecordSerializer>::pthreadCondWait(unsigned ins, int &error, 
                pthread_cond_t *cv, pthread_mutex_t *mu){
  typedef RecordSerializer _S;
      int ret;
  SCHED_TIMER_START;
  pthread_mutex_unlock(mu);

  SCHED_TIMER_FAKE_END(syncfunc::pthread_cond_wait, (uint64_t)cv, (uint64_t)mu);
  errno = error;
  pthread_cond_wait(cv, RecordSerializer::getLock());
  error = errno;
  sched_time = update_time();

  while((ret=pthread_mutex_trylock(mu))) {
    assert(ret==EBUSY && "failed sync calls are not yet supported!");
    syncWait(mu);
  }
  nturn = RecordSerializer::incTurnCount();
  SCHED_TIMER_END(syncfunc::pthread_cond_wait, (uint64_t)cv, (uint64_t)mu);

  return 0;
}

template <>
int RecorderRT<RecordSerializer>::pthreadCondTimedWait(unsigned ins, int &error, 
    pthread_cond_t *cv, pthread_mutex_t *mu, const struct timespec *abstime){
  typedef RecordSerializer _S;
  SCHED_TIMER_START;
  pthread_mutex_unlock(mu);
  SCHED_TIMER_FAKE_END(syncfunc::pthread_cond_timedwait, (uint64_t)cv, (uint64_t)mu);

  errno = error;
  int ret = pthread_cond_timedwait(cv, _S::getLock(), abstime);
  error = errno;
  if(ret == ETIMEDOUT) {
    dprintf("%d timed out from timedwait\n", _S::self());
  }
  assert((ret==0||ret==ETIMEDOUT) && "failed sync calls are not yet supported!");

  pthreadMutexLockHelper(mu);
  SCHED_TIMER_END(syncfunc::pthread_cond_timedwait, (uint64_t)cv, (uint64_t)mu, (uint64_t) ret);
 
  return ret;
}

template <>
int RecorderRT<RecordSerializer>::pthreadCondSignal(unsigned ins, int &error, 
                                                 pthread_cond_t *cv){
  typedef RecordSerializer _S;

  SCHED_TIMER_START;
  errno = error;
  pthread_cond_signal(cv);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_cond_signal, (uint64_t)cv);

  return 0;
}

template <>
int RecorderRT<RecordSerializer>::pthreadCondBroadcast(unsigned ins, int &error, 
                                                    pthread_cond_t*cv){
  typedef RecordSerializer _S;

  SCHED_TIMER_START;
  errno = error;
  pthread_cond_broadcast(cv);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_cond_broadcast, (uint64_t)cv);

  return 0;
}

template <typename _S>
bool RecorderRT<_S>::regularFile(int fd) {
  struct stat st;
  fstat(fd, &st);
  // If it is neither a socket, nor a fifo, then it is regular file (not a inter-process communication media).
  return ((S_IFSOCK != (st.st_mode & S_IFMT)) && (S_IFIFO != (st.st_mode & S_IFMT)));
}

template <typename _S>
int RecorderRT<_S>::__accept(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  BLOCK_TIMER_START(accept, ins, error, sockfd, cliaddr, addrlen);
  int ret = Runtime::__accept(ins, error, sockfd, cliaddr, addrlen);
  int from_port = 0;
  int to_port = 0;
  if (options::log_sync) {
    to_port = ((struct sockaddr_in *)cliaddr)->sin_port;
    struct sockaddr_in servaddr;
    socklen_t len = sizeof(servaddr);
    getsockname(sockfd, (struct sockaddr *)&servaddr, &len);
    from_port = servaddr.sin_port;
  }

  BLOCK_TIMER_END(syncfunc::accept, (uint64_t)ret, (uint64_t)from_port, (uint64_t) to_port);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__accept4(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags)
{
  BLOCK_TIMER_START(accept4, ins, error, sockfd, cliaddr, addrlen, flags);
  int ret = Runtime::__accept4(ins, error, sockfd, cliaddr, addrlen, flags);
  BLOCK_TIMER_END(syncfunc::accept4, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__connect(unsigned ins, int &error, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  BLOCK_TIMER_START(connect, ins, error, sockfd, serv_addr, addrlen);
  int ret = Runtime::__connect(ins, error, sockfd, serv_addr, addrlen);
  int from_port = 0;
  int to_port = 0;
  if (options::log_sync) {
    from_port = ((const struct sockaddr_in*) serv_addr)->sin_port;
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    getsockname(sockfd, (struct sockaddr *)&cliaddr, &len);
    to_port = cliaddr.sin_port;
  }

  BLOCK_TIMER_END(syncfunc::connect, (uint64_t) sockfd, (uint64_t) from_port, (uint64_t) to_port, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__send(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags)
{
  /* Even it is non-blocking operation, we use BLOCK_* instead of SCHED_*, 
    because this operation can be involved by other systematic testing tools to 
    explore non-deterministic order. */
  BLOCK_TIMER_START(send, ins, error, sockfd, buf, len, flags);
  int ret = Runtime::__send(ins, error, sockfd, buf, len, flags);
  BLOCK_TIMER_END(syncfunc::send, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__sendto(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  BLOCK_TIMER_START(sendto, ins, error, sockfd, buf, len, flags, dest_addr, addrlen);
  int ret = Runtime::__sendto(ins, error, sockfd, buf, len, flags, dest_addr, addrlen);
  BLOCK_TIMER_END(syncfunc::sendto, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__sendmsg(unsigned ins, int &error, int sockfd, const struct msghdr *msg, int flags)
{
  BLOCK_TIMER_START(sendmsg, ins, error, sockfd, msg, flags);
  int ret = Runtime::__sendmsg(ins, error, sockfd, msg, flags);
  BLOCK_TIMER_END(syncfunc::sendmsg, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recv(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags)
{
  BLOCK_TIMER_START(recv, ins, error, sockfd, buf, len, flags);
  ssize_t ret = Runtime::__recv(ins, error, sockfd, buf, len, flags);
  BLOCK_TIMER_END(syncfunc::recv, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recvfrom(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  BLOCK_TIMER_START(recvfrom, ins, error, sockfd, buf, len, flags, src_addr, addrlen);
  ssize_t ret = Runtime::__recvfrom(ins, error, sockfd, buf, len, flags, src_addr, addrlen);
  BLOCK_TIMER_END(syncfunc::recvfrom, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recvmsg(unsigned ins, int &error, int sockfd, struct msghdr *msg, int flags)
{
  BLOCK_TIMER_START(recvmsg, ins, error, sockfd, msg, flags);
  ssize_t ret = Runtime::__recvmsg(ins, error, sockfd, msg, flags);
  BLOCK_TIMER_END(syncfunc::recvmsg, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__read(unsigned ins, int &error, int fd, void *buf, size_t count)
{
  // First, handle regular IO.
  if (options::RR_ignore_rw_regular_file && regularFile(fd))
    return read(fd, buf, count);  // Directly call the libc read() for regular IO.

  // Second, handle inter-process IO.
  BLOCK_TIMER_START(read, ins, error, fd, buf, count);
  ssize_t ret = Runtime::__read(ins, error, fd, buf, count);
  BLOCK_TIMER_END(syncfunc::read, (uint64_t) fd, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__write(unsigned ins, int &error, int fd, const void *buf, size_t count)
{
  // First, handle regular IO.
  if (options::RR_ignore_rw_regular_file && regularFile(fd)) {
    dprintf("RecorderRT<_S>::__write ignores regular file %d\n", fd);
    return write(fd, buf, count);  // Directly call the libc write() for regular IO.
  }

  // Second, handle inter-process IO.
  /* Even it is non-blocking operation, we use BLOCK_* instead of SCHED_*, 
    because this operation can be involved by other systematic testing tools to 
    explore non-deterministic order. */
  BLOCK_TIMER_START(write, ins, error, fd, buf, count);
  dprintf("RecorderRT<_S>::__write handles inter-process file %d\n", fd);
  ssize_t ret = Runtime::__write(ins, error, fd, buf, count);
  BLOCK_TIMER_END(syncfunc::write, (uint64_t) fd, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__pread(unsigned ins, int &error, int fd, void *buf, size_t count, off_t offset)
{
  // First, handle regular IO.
  if (options::RR_ignore_rw_regular_file && regularFile(fd))
    return pread(fd, buf, count, offset);  // Directly call the libc pread() for regular IO.

  // Second, handle inter-process IO.
  BLOCK_TIMER_START(pread, ins, error, fd, buf, count, offset);
  ssize_t ret = Runtime::__pread(ins, error, fd, buf, count, offset);
  BLOCK_TIMER_END(syncfunc::pread, (uint64_t) fd, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__pwrite(unsigned ins, int &error, int fd, const void *buf, size_t count, off_t offset)
{
  // First, handle regular IO.
  if (options::RR_ignore_rw_regular_file && regularFile(fd))
    return pwrite(fd, buf, count, offset);  // Directly call the libc pwrite() for regular IO.

  // Second, handle inter-process IO.
  BLOCK_TIMER_START(pwrite, ins, error, fd, buf, count, offset);
  ssize_t ret = Runtime::__pwrite(ins, error, fd, buf, count, offset);
  BLOCK_TIMER_END(syncfunc::pwrite, (uint64_t) fd, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__select(unsigned ins, int &error, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  BLOCK_TIMER_START(select, ins, error, nfds, readfds, writefds, exceptfds, timeout);
  int ret = Runtime::__select(ins, error, nfds, readfds, writefds, exceptfds, timeout);
  BLOCK_TIMER_END(syncfunc::select, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__epoll_wait(unsigned ins, int &error, int epfd, struct epoll_event *events, int maxevents, int timeout)
{  
  BLOCK_TIMER_START(epoll_wait, ins, error, epfd, events, maxevents, timeout);
  int ret = Runtime::__epoll_wait(ins, error, epfd, events, maxevents, timeout);
  BLOCK_TIMER_END(syncfunc::epoll_wait, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__epoll_create(unsigned ins, int &error, int size)
{  
  BLOCK_TIMER_START(epoll_create, ins, error, size);
  int ret = Runtime::__epoll_create(ins, error, size);
  BLOCK_TIMER_END(syncfunc::epoll_create, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__epoll_ctl(unsigned ins, int &error, int epfd, int op, int fd, struct epoll_event *event)
{  
  BLOCK_TIMER_START(epoll_ctl, ins, error, epfd, op, fd, event);
  int ret = Runtime::__epoll_ctl(ins, error, epfd, op, fd, event);
  BLOCK_TIMER_END(syncfunc::epoll_ctl, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__poll(unsigned ins, int &error, struct pollfd *fds, nfds_t nfds, int timeout)
{
  BLOCK_TIMER_START(poll, ins, error, fds, nfds, timeout);
  int ret = Runtime::__poll(ins, error, fds, nfds, timeout);
  BLOCK_TIMER_END(syncfunc::poll, (uint64_t)fds, (uint64_t)nfds, (uint64_t)timeout, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__bind(unsigned ins, int &error, int socket, const struct sockaddr *address, socklen_t address_len)
{
  BLOCK_TIMER_START(bind, ins, error, socket, address, address_len);
  int ret = Runtime::__bind(ins, error, socket, address, address_len);
  BLOCK_TIMER_END(syncfunc::bind, (uint64_t)socket, (uint64_t)address, (uint64_t)address_len, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__sigwait(unsigned ins, int &error, const sigset_t *set, int *sig)
{
  BLOCK_TIMER_START(sigwait, ins, error, set, sig);
  int ret = Runtime::__sigwait(ins, error, set, sig);
  BLOCK_TIMER_END(syncfunc::sigwait, (uint64_t) ret);
  return ret;
}

template <typename _S>
char *RecorderRT<_S>::__fgets(unsigned ins, int &error, char *s, int size, FILE *stream)
{
  // First, handle regular IO.
  if (options::RR_ignore_rw_regular_file && regularFile(fileno(stream)))
    return fgets(s, size, stream);  // Directly call the libc fgets() for regular IO.

  // Second, handle inter-process IO.
  BLOCK_TIMER_START(fgets, ins, error, s, size, stream);
  char * ret = Runtime::__fgets(ins, error, s, size, stream);
  BLOCK_TIMER_END(syncfunc::fgets, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__kill(unsigned ins, int &error, pid_t pid, int sig)
{
  BLOCK_TIMER_START(kill, ins, error, pid, sig);
  int ret = Runtime::__kill(ins, error, pid, sig);
  BLOCK_TIMER_END(syncfunc::kill, (uint64_t)ret);
  return ret;
}

template <typename _S>
pid_t RecorderRT<_S>::__fork(unsigned ins, int &error)
{
  dprintf("pid %d enters fork\n", getpid());
  pid_t ret;

  if (options::log_sync)
    Logger::the->flush(); // so child process won't write it again

  /* Although this is inter-process operation, and we need to involve dbug
    tool (debug needs to register/unregister threads based on fork()), we do
    not need to switch from SCHED_TIMER_START to BLOCK_TIMER_START, 
    because at this moment the child thread is not alive yet, so dbug could not 
    enforce any order between child and parent processes. And, we need this
    sched_* scheduling way to update the runq and waitq of parent
    and child processes safely. */
  SCHED_TIMER_START;
  ret = Runtime::__fork(ins, error);
  if(ret == 0) {
    // child process returns from fork; re-initializes scheduler and logger state
    Logger::threadEnd(); // close log
    Logger::threadBegin(_S::self()); // re-open log
    assert(!sem_init(&thread_begin_sem, 0, 0));
    assert(!sem_init(&thread_begin_done_sem, 0, 0));
    _S::childForkReturn();
  } else
    assert(ret > 0);
  SCHED_TIMER_END(syncfunc::fork, (uint64_t) ret);

  // FIXME: this is gross.  idle thread should be part of RecorderRT
  if (ret == 0 && options::launch_idle_thread) {
    Space::exitSys();
    pthread_cond_init(&idle_cond, NULL);
    pthread_mutex_init(&idle_mutex, NULL);
    int res = tern_pthread_create(0xdead0000, &idle_th, NULL, idle_thread, NULL);
    assert(res == 0 && "tern_pthread_create failed!");
    Space::enterSys();
  }

  dprintf("pid %d leaves fork\n", getpid());
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__execv(unsigned ins, int &error, const char *path, char *const argv[])
{
  if (options::log_sync)
    Logger::the->flush(); // so child process won't write it again
    
  int ret = 0;
  SCHED_TIMER_START;
  nturn = 0; // Just avoid compiler warning.
  ret = Runtime::__execv(ins, error, path, argv);
  assert(false && "execv failed.");

  return ret;
}

template <typename _S>
pid_t RecorderRT<_S>::__wait(unsigned ins, int &error, int *status)
{
  BLOCK_TIMER_START(wait, ins, error, status);
  pid_t ret = Runtime::__wait(ins, error, status);
  BLOCK_TIMER_END(syncfunc::wait, (uint64_t)*status, (uint64_t)ret);
  return ret;
}

template <typename _S>
pid_t RecorderRT<_S>::__waitpid(unsigned ins, int &error, pid_t pid, int *status, int options)
{
  BLOCK_TIMER_START(waitpid, ins, error, pid, status, options);
  pid_t ret = Runtime::__waitpid(ins, error, pid, status, options);
  BLOCK_TIMER_END(syncfunc::waitpid, (uint64_t)pid, (uint64_t)*status, (uint64_t)options, (uint64_t)ret);
  return ret;
}


template <typename _S>
int RecorderRT<_S>::schedYield(unsigned ins, int &error)
{
  int ret;
  if (options::enforce_non_det_annotations && inNonDet) {
    // Do not need to count nNonDetPthreadSync for this op.
    //fprintf(stderr, "non-det yield start tid %d...\n", _S::self());  
    ret = Runtime::__sched_yield(ins, error);
    //fprintf(stderr, "non-det yield end tid %d...\n", _S::self());  
    return ret;
  }
  SCHED_TIMER_START;
  ret = sched_yield();
  SCHED_TIMER_END(syncfunc::sched_yield, (uint64_t)ret);
  return ret;
}

// TODO: right now we treat sleep functions just as a turn; should convert
// real time to logical time
template <typename _S>
unsigned int RecorderRT<_S>::__sleep(unsigned ins, int &error, unsigned int seconds)
{
#ifdef XTERN_PLUS_DBUG
  BLOCK_TIMER_START(sleep, ins, error, seconds);
  unsigned int ret = Runtime::__sleep(ins, error, seconds);
  BLOCK_TIMER_END(syncfunc::sleep, (uint64_t)ret);
  return ret;
#else
  struct timespec ts = {seconds, 0};
  SCHED_TIMER_START;
  // must call _S::getTurnCount with turn held
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(&ts);
  _S::wait(NULL, timeout);
  SCHED_TIMER_END(syncfunc::sleep, (uint64_t) seconds * 1000000000);
  if (options::exec_sleep)
    ::sleep(seconds);
  return 0;
#endif
}

template <typename _S>
int RecorderRT<_S>::__usleep(unsigned ins, int &error, useconds_t usec)
{
#ifdef XTERN_PLUS_DBUG
  BLOCK_TIMER_START(usleep, ins, error, usec);
  fprintf(stderr, "dbug usleep...\n");
  unsigned int ret = Runtime::__usleep(ins, error, usec);
  BLOCK_TIMER_END(syncfunc::usleep, (uint64_t)ret);
  return ret;
#else
  struct timespec ts = {0, 1000*usec};
  SCHED_TIMER_START;
  // must call _S::getTurnCount with turn held
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(&ts);
  _S::wait(NULL, timeout);
  SCHED_TIMER_END(syncfunc::usleep, (uint64_t) usec * 1000);
  if (options::exec_sleep)
    ::usleep(usec);
  return 0;
#endif
}

template <typename _S>
int RecorderRT<_S>::__nanosleep(unsigned ins, int &error, 
                              const struct timespec *req,
                              struct timespec *rem)
{
#ifdef XTERN_PLUS_DBUG
  BLOCK_TIMER_START(nanosleep, ins, error, req, rem);
  fprintf(stderr, "dbug nanosleep...\n");
  unsigned int ret = Runtime::__nanosleep(ins, error, req, rem);
  BLOCK_TIMER_END(syncfunc::nanosleep, (uint64_t)ret);
  return ret;
#else
 SCHED_TIMER_START;
   // must call _S::getTurnCount with turn held
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(req);
  _S::wait(NULL, timeout);
  uint64_t nsec = !req ? 0 : (req->tv_sec * 1000000000 + req->tv_nsec); 
  SCHED_TIMER_END(syncfunc::nanosleep, (uint64_t) nsec);
  if (options::exec_sleep)
    ::nanosleep(req, rem);
  return 0;
#endif
}

template <typename _S>
int RecorderRT<_S>::__socket(unsigned ins, int &error, int domain, int type, int protocol)
{
  BLOCK_TIMER_START(socket, ins, error, domain, type, protocol);
  int ret = Runtime::__socket(ins, error, domain, type, protocol);
  BLOCK_TIMER_END(syncfunc::socket, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__listen(unsigned ins, int &error, int sockfd, int backlog)
{
  BLOCK_TIMER_START(listen, ins, error, sockfd, backlog);
  int ret = Runtime::__listen(ins, error, sockfd, backlog);
  BLOCK_TIMER_END(syncfunc::listen, (uint64_t)sockfd, (uint64_t)backlog, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__shutdown(unsigned ins, int &error, int sockfd, int how)
{
  BLOCK_TIMER_START(shutdown, ins, error, sockfd, how);
  int ret = Runtime::__shutdown(ins, error, sockfd, how);
  BLOCK_TIMER_END(syncfunc::shutdown, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__getpeername(unsigned ins, int &error, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  return Runtime::__getpeername(ins, error, sockfd, addr, addrlen);
}

template <typename _S>
int RecorderRT<_S>::__getsockopt(unsigned ins, int &error, int sockfd, int level, int optname,
                      void *optval, socklen_t *optlen)
{
  BLOCK_TIMER_START(getsockopt, ins, error, sockfd, level, optname, optval, optlen);
  int ret = Runtime::__getsockopt(ins, error, sockfd, level, optname, optval, optlen);
  BLOCK_TIMER_END(syncfunc::getsockopt, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__setsockopt(unsigned ins, int &error, int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen)
{
  BLOCK_TIMER_START(setsockopt, ins, error, sockfd, level, optname, optval, optlen);
  int ret = Runtime::__setsockopt(ins, error, sockfd, level, optname, optval, optlen);
  BLOCK_TIMER_END(syncfunc::setsockopt, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__pipe(unsigned ins, int &error, int pipefd[2])
{
  BLOCK_TIMER_START(pipe, ins, error, pipefd);
  int ret = Runtime::__pipe(ins, error, pipefd);
  BLOCK_TIMER_END(syncfunc::pipe, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__fcntl(unsigned ins, int &error, int fd, int cmd, void *arg)
{
  BLOCK_TIMER_START(fcntl, ins, error, fd, cmd, arg);
  int ret = Runtime::__fcntl(ins, error, fd, cmd, arg);
  BLOCK_TIMER_END(syncfunc::fcntl, (uint64_t)ret);
  return ret;
}


template <typename _S>
int RecorderRT<_S>::__close(unsigned ins, int &error, int fd)
{
  // First, handle regular IO.
  if (options::RR_ignore_rw_regular_file && regularFile(fd))
    return close(fd);  // Directly call the libc close() for regular IO.

  // Second, handle inter-process IO.
  BLOCK_TIMER_START(close, ins, error, fd);
  int ret = Runtime::__close(ins, error, fd);
  BLOCK_TIMER_END(syncfunc::close, (uint64_t)fd, (uint64_t)ret);
  // For servers, print stat here, at this point it could be non-det but it is fine, network is non-det anyway.
  if (options::record_runtime_stat)
    stat.print();  
  return ret;
}

template <>
unsigned int RecorderRT<RecordSerializer>::__sleep(unsigned ins, int &error, unsigned int seconds)
{
  typedef Runtime _P;
  return _P::__sleep(ins, error, seconds);
}

template <>
int RecorderRT<RecordSerializer>::__usleep(unsigned ins, int &error, useconds_t usec)
{
  typedef Runtime _P;
  return _P::__usleep(ins, error, usec);
}

template <>
int RecorderRT<RecordSerializer>::__nanosleep(unsigned ins, int &error, 
                                            const struct timespec *req,
                                            struct timespec *rem)
{
  typedef Runtime _P;
  return _P::__nanosleep(ins, error, req, rem);
}

template <typename _S>
time_t RecorderRT<_S>::__time(unsigned ins, int &error, time_t *t)
{
  return Runtime::__time(ins, error, t);
}

template <typename _S>
int RecorderRT<_S>::__clock_getres(unsigned ins, int &error, clockid_t clk_id, struct timespec *res)
{
  return Runtime::__clock_getres(ins, error, clk_id, res);
}

template <typename _S>
int RecorderRT<_S>::__clock_gettime(unsigned ins, int &error, clockid_t clk_id, struct timespec *tp)
{
  return Runtime::__clock_gettime(ins, error, clk_id, tp);
}

template <typename _S>
int RecorderRT<_S>::__clock_settime(unsigned ins, int &error, clockid_t clk_id, const struct timespec *tp)
{
  return Runtime::__clock_settime(ins, error, clk_id, tp);
}

template <typename _S>
int RecorderRT<_S>::__gettimeofday(unsigned ins, int &error, struct timeval *tv, struct timezone *tz)
{
  return Runtime::__gettimeofday(ins, error, tv, tz);
}

template <typename _S>
int RecorderRT<_S>::__settimeofday(unsigned ins, int &error, const struct timeval *tv, const struct timezone *tz)
{
  return Runtime::__settimeofday(ins, error, tv, tz);
}

template <typename _S>
struct hostent *RecorderRT<_S>::__gethostbyname(unsigned ins, int &error, const char *name)
{
  BLOCK_TIMER_START(gethostbyname, ins, error, name);
  struct hostent *ret = Runtime::__gethostbyname(ins, error, name);
  BLOCK_TIMER_END(syncfunc::gethostbyname, (uint64_t)ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__gethostbyname_r(unsigned ins, int &error, const char *name, struct hostent *ret,
  char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
  BLOCK_TIMER_START(gethostbyname_r, ins, error, name, ret, buf, buflen, result, h_errnop);
  int ret2 = Runtime::__gethostbyname_r(ins, error, name, ret, buf, buflen, result, h_errnop);
  BLOCK_TIMER_END(syncfunc::gethostbyname_r, (uint64_t)ret2);
  return ret2;
}

template <typename _S>
int RecorderRT<_S>::__getaddrinfo(unsigned ins, int &error, const char *node, const char *service, const struct addrinfo *hints,
struct addrinfo **res)
{
  BLOCK_TIMER_START(getaddrinfo, ins, error, node, service, hints, res);
  int ret2 = Runtime::__getaddrinfo(ins, error, node, service, hints, res);
  BLOCK_TIMER_END(syncfunc::getaddrinfo, (uint64_t)ret2);
  return ret2;
}

template <typename _S>
void RecorderRT<_S>::__freeaddrinfo(unsigned ins, int &error, struct addrinfo *res)
{
  BLOCK_TIMER_START(freeaddrinfo, ins, error, res);
  Runtime::__freeaddrinfo(ins, error, res);
  BLOCK_TIMER_END(syncfunc::freeaddrinfo, (uint64_t)ret2);
}

template <typename _S>
struct hostent *RecorderRT<_S>::__gethostbyaddr(unsigned ins, int &error, const void *addr, int len, int type)
{
  BLOCK_TIMER_START(gethostbyaddr, ins, error, addr, len, type);
  struct hostent *ret = Runtime::__gethostbyaddr(ins, error, addr, len, type);
  BLOCK_TIMER_END(syncfunc::gethostbyaddr, (uint64_t)ret);
  return ret;
}

template <typename _S>
char *RecorderRT<_S>::__inet_ntoa(unsigned ins, int &error, struct in_addr in) {
  BLOCK_TIMER_START(inet_ntoa, ins, error, in);
  char * ret = Runtime::__inet_ntoa(ins, error, in);
  BLOCK_TIMER_END(syncfunc::inet_ntoa, (uint64_t)ret);
  return ret;
}

template <typename _S>
char *RecorderRT<_S>::__strtok(unsigned ins, int &error, char * str, const char * delimiters) {
  BLOCK_TIMER_START(strtok, ins, error, str, delimiters);
  char * ret = Runtime::__strtok(ins, error, str, delimiters);
  BLOCK_TIMER_END(syncfunc::strtok, (uint64_t)ret);
  return ret;
}

//////////////////////////////////////////////////////////////////////////
// Replay Runtime
//
// optimization: can skip synchronization operations.  three different
// approaches
//
//  - skip no synchronization operations
//
//  - skip sleep or barrier wait operations
//
//  - skip all synchronization operations

/// pthread_cond_wait(&cv, &mu)
///   getTurn();
///   advance iter
///   putTurn()
///   pthread_cond_wait(&cv, &mu); // okay to do it here, as we don't care
///                                // when we'll be woken up, as long as
///                                // we enforce the order later before
///                                // exiting this hook
///
///   pthread_mutex_unlock(&mu);   // unlock in case we grabbed this mutex
///                                // prematurely; see example below
///
///   getTurn();
///   advance iter
///   putTurn()
///
///   pthread_mutex_lock(&mu);  // safe to lock here since we have this
///                             // mutex according to the schedule, so the
///                             // log will not have another
///                             // pthread_mutex_lock record before we call
///                             // unlock.
///
/// Why need pthread_mutex_unlock(&mu) in above code?  consider the schedule
///
///    t1             t2          t3
///  cond_wait
///                 lock
///                 signal
///                 lock
///                              lock
///                              unlock
///
/// if in replay, t1 grabs lock first before t3, then replay will deadlock.

} // namespace tern
