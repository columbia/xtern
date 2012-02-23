// Authors: Junfeng Yang (junfeng@cs.columbia.edu).  Refactored from
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
#include "tern/runtime/record-log.h"
#include "tern/runtime/record-runtime.h"
#include "tern/runtime/record-scheduler.h"
#include "tern/options.h"
#include "signal.h"
#include "helper.h"
#include <fstream>
#include <map>
#include <sys/types.h>

// FIXME: these should be in tern/config.h
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE<199309L)
// Need this for clock_gettime
#  define _POSIX_C_SOURCE (199309L)
#endif

//#define _DEBUG_RECORDER

#ifdef _DEBUG_RECORDER
#  define dprintf(fmt...) do {                   \
     fprintf(stderr, "[%d]", self());            \
     fprintf(stderr, fmt);                       \
     fflush(stderr);                             \
  } while(0)
#else
#  define dprintf(fmt...)
#endif

//#define __TIME_MEASURE 1

using namespace std;

namespace tern {

static __thread timespec my_time;

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
  clock_gettime(CLOCK_MONOTONIC_RAW , &start_time);
  timespec ret = time_diff(my_time, start_time);
  my_time = start_time; 
  return ret;
}
  
void InstallRuntime() {
//  if (options::runtime_type == "RRuntime")
//    Runtime::the = new RRuntime();
//  else 
  if (options::runtime_type == "RR")
    //Runtime::the = new RecorderRT<RRSchedulerCV>;
    Runtime::the = new RecorderRT<RRScheduler>;
  else if (options::runtime_type == "SeededRR") {
    RecorderRT<SeededRRScheduler> *rt = new RecorderRT<SeededRRScheduler>;
    static_cast<SeededRRScheduler*>(rt)->setSeed(options::scheduler_seed);
    Runtime::the = rt;
  } else if (options::runtime_type == "FCFS")
    Runtime::the = new RecorderRT<RecordSerializer>;
  assert(Runtime::the && "can't create runtime!");
}

template <typename _S>
int RecorderRT<_S>::wait(void *chan, unsigned timeout) {
  return _S::wait(chan, timeout);
}

template <typename _S>
void RecorderRT<_S>::signal(void *chan, bool all) {
  _S::signal(chan, all);
}

template <typename _S>
int RecorderRT<_S>::absTimeToTurn(const struct timespec *abstime)
{
  // TODO: convert physical time to logical time (number of turns)
  return _S::getTurnCount() + 30; //rand() % 10;
}

template <typename _S>
int RecorderRT<_S>::relTimeToTurn(const struct timespec *reltime)
{
  // TODO: convert physical time to logical time (number of turns)
  if (!reltime) return 0;
  uint64_t ns = reltime->tv_sec;
  ns = ns * 1000000000 + reltime->tv_nsec;
  int64_t ret64 = (ns / 1000 / options::nanosec_per_turn + 1) * _S::nthread;
  const int MAX_REL = 1000000;
  int ret =  (ret64 > MAX_REL) ? MAX_REL : (int) ret64;
  ret = ret < 30 * _S::nthread + 1 ? 30 * _S::nthread + 1 : ret;
  int tmp = rand() % 100 * _S::nthread;
  fprintf(stderr, "computed turn = %d, tmp = %d\n", ret, tmp);
  //return tmp;
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

#define BLOCK_TIMER_START \
  timespec app_time = update_time(); \
  _S::block(); \
  timespec sched_block_time = update_time();

#define BLOCK_TIMER_END(syncop, ...) \
  int backup_errno = errno; \
  timespec syscall_time = update_time(); \
  _S::wakeup(); \
  timespec sched_wakeup_time = update_time(); \
  timespec sched_time = { \
    sched_wakeup_time.tv_sec + sched_block_time.tv_sec, \
    sched_wakeup_time.tv_nsec + sched_block_time.tv_nsec \
    }; \
  Logger::the->logSync(ins, (syncop), _S::getTurnCount(), app_time, syscall_time, sched_time, true, __VA_ARGS__); \
  errno = backup_errno; 

#define SCHED_TIMER_START \
  unsigned nturn; \
  timespec app_time = update_time(); \
  _S::getTurn(); \
  timespec sched_time = update_time();

#define SCHED_TIMER_END_COMMON(syncop, ...) \
  int backup_errno = errno; \
  timespec syscall_time = update_time(); \
  nturn = _S::incTurnCount(); \
  Logger::the->logSync(ins, (syncop), nturn = _S::getTurnCount(), app_time, syscall_time, sched_time, true, __VA_ARGS__);
   
#define SCHED_TIMER_END(syncop, ...) \
  SCHED_TIMER_END_COMMON(syncop, __VA_ARGS__); \
  _S::putTurn();\
  errno = backup_errno; 

#define SCHED_TIMER_THREAD_END(syncop, ...) \
  SCHED_TIMER_END_COMMON(syncop, __VA_ARGS__); \
  _S::putTurn(true);\
  errno = backup_errno; 
  
#define SCHED_TIMER_FAKE_END(syncop, ...) \
  nturn = _S::incTurnCount(); \
  timespec fake_time = update_time(); \
  Logger::the->logSync(ins, syncop, nturn, app_time, fake_time, sched_time, /* before */ false, __VA_ARGS__); 
  
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
  SCHED_TIMER_END(syncfunc::tern_thread_end, (uint64_t)th);
  
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
  SCHED_TIMER_START;
  // NOTE: can actually use if(!_S::zombie(th)) for DMT schedulers because
  // their wait() won't return until some thread signal().
  while(!_S::zombie(th))
    wait((void*)th);
  errno = error;
  ret = pthread_join(th, rv);
  error = errno;
  assert(!ret && "failed sync calls are not yet supported!");
  _S::join(th);
  
  SCHED_TIMER_END(syncfunc::pthread_join, (uint64_t)th);

  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexInit(unsigned ins, int &error, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
{
  int ret;
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
    ret = wait(mu, timeout);
    if(ret == ETIMEDOUT)
      return ETIMEDOUT;
  }
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexLock(unsigned ins, int &error, pthread_mutex_t *mu) {
  SCHED_TIMER_START;
  errno = error;
  pthreadMutexLockHelper(mu);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_mutex_lock, (uint64_t)mu);
  return 0;
}

/// instead of looping to get lock as how we implement the regular lock(),
/// here just trylock once and return.  this preserves the semantics of
/// trylock().
template <typename _S>
int RecorderRT<_S>::pthreadMutexTryLock(unsigned ins, int &error, pthread_mutex_t *mu) {
  int ret;
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
  if(abstime == NULL)
    return pthreadMutexLock(ins, error, mu);

  timespec cur_time, rel_time;
  clock_gettime(CLOCK_MONOTONIC_RAW , &cur_time);
  rel_time = time_diff(cur_time, *abstime);

  SCHED_TIMER_START;
  unsigned timeout = relTimeToTurn(&rel_time);
  errno = error;
  int ret = pthreadMutexLockHelper(mu, timeout);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_mutex_timedlock, (uint64_t)mu, (uint64_t) ret);
 
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexUnlock(unsigned ins, int &error, pthread_mutex_t *mu){
  int ret;
  SCHED_TIMER_START;

  errno = error;
  ret = pthread_mutex_unlock(mu);
  error = errno;
  assert(!ret && "failed sync calls are not yet supported!");
  signal(mu);
 
  SCHED_TIMER_END(syncfunc::pthread_mutex_unlock, (uint64_t)mu, (uint64_t) ret);

  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadBarrierInit(unsigned ins, int &error, pthread_barrier_t *barrier,
                                       unsigned count) {
  int ret;
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
/// we want to avoid the head of queue block, so must call wait(ba) and
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
  /// Note: the signal() operation has to be done while the thread has the
  /// turn; otherwise two independent signal() operations on two
  /// independent barriers can be nondeterministic.  e.g., suppose two
  /// barriers ba1 and ba2 each has count 1.
  ///
  ///       t0                        t1
  ///
  /// getTurn()
  /// wait(ba1);
  ///                         getTurn()
  ///                         wait(ba1);
  /// barrier_wait(ba1)
  ///                         barrier_wait(ba2)
  /// signal(ba1)             signal(ba2)
  ///
  /// these two signal() ops can be nondeterministic, causing threads to be
  /// added to the activeq in different orders
  ///
  
    
  
  int ret;
  SCHED_TIMER_START;
  SCHED_TIMER_FAKE_END(syncfunc::pthread_barrier_wait, (uint64_t)barrier);
  
  barrier_map::iterator bi = barriers.find(barrier);
  assert(bi!=barriers.end() && "barrier is not initialized!");
  barrier_t &b = bi->second;

  ++ b.narrived;
  assert(b.narrived <= b.count && "barrier overflow!");
  if(b.count == b.narrived) {
    b.narrived = 0; // barrier may be reused
    _S::signal(barrier, /*all=*/true);
    // according to the man page of pthread_barrier_wait, one of the
    // waiters should return PTHREAD_BARRIER_SERIAL_THREAD, instead of 0
    ret = PTHREAD_BARRIER_SERIAL_THREAD;
    _S::putTurn();    
    _S::getTurn();
  } else {
    ret = 0;
    _S::wait(barrier);
  }
  sched_time = update_time();

  SCHED_TIMER_END(syncfunc::pthread_barrier_wait, (uint64_t)barrier);

  return ret;
}

// FIXME: the handling of the EBUSY case seems gross
template <typename _S>
int RecorderRT<_S>::pthreadBarrierDestroy(unsigned ins, int &error, 
                                          pthread_barrier_t *barrier) {
  int ret;
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
///   signal(&mu);
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
///   signal(&mu);
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
///   signal(&mu);
///   wait(&cv);
///   putTurn();
///
///   getTurn();
///   pthreadMutexLockHelper(&mu);
///   putTurn();
///
/// pthread_cond_signal(&cv):
///   getTurn();
///   signal(&cv);
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
  SCHED_TIMER_START;
  pthread_mutex_unlock(mu);
  _S::signal(mu);

  SCHED_TIMER_FAKE_END(syncfunc::pthread_cond_wait, (uint64_t)cv, (uint64_t)mu);
  
  _S::wait(cv);
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
  clock_gettime(CLOCK_MONOTONIC_RAW , &cur_time);
  rel_time = time_diff(cur_time, *abstime);

  int ret;
  SCHED_TIMER_START;
  unsigned timeout = relTimeToTurn(&rel_time);
  pthread_mutex_unlock(mu);
    
  SCHED_TIMER_FAKE_END(syncfunc::pthread_cond_timedwait, (uint64_t)cv, (uint64_t)mu, (uint64_t) 0);

  _S::signal(mu);
  saved_ret = ret = _S::wait(cv, timeout);

  sched_time = update_time();
  errno = error;
  pthreadMutexLockHelper(mu);
  error = errno;
  SCHED_TIMER_END(syncfunc::pthread_cond_timedwait, (uint64_t)cv, (uint64_t)mu, (uint64_t) saved_ret);

  return saved_ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondSignal(unsigned ins, int &error, pthread_cond_t *cv){

  SCHED_TIMER_START;
  _S::signal(cv);
  SCHED_TIMER_END(syncfunc::pthread_cond_signal, (uint64_t)cv);

  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondBroadcast(unsigned ins, int &error, pthread_cond_t*cv){
  SCHED_TIMER_START;
  _S::signal(cv, /*all=*/true);
  SCHED_TIMER_END(syncfunc::pthread_cond_broadcast, (uint64_t)cv);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::semWait(unsigned ins, int &error, sem_t *sem) {
  int ret;
  SCHED_TIMER_START;
  while((ret=sem_trywait(sem)) != 0) {
    // WTH? pthread_mutex_trylock returns EBUSY if lock is held, yet
    // sem_trywait returns -1 and sets errno to EAGAIN if semaphore is not
    // available
    assert(errno==EAGAIN && "failed sync calls are not yet supported!");
    wait(sem);
  }
  SCHED_TIMER_END(syncfunc::sem_wait, (uint64_t)sem);

  return 0;
}

template <typename _S>
int RecorderRT<_S>::semTryWait(unsigned ins, int &error, sem_t *sem) {
  int ret;
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
  clock_gettime(CLOCK_MONOTONIC_RAW , &cur_time);
  rel_time = time_diff(cur_time, *abstime);
  
  int ret;
  SCHED_TIMER_START;
  
  unsigned timeout = relTimeToTurn(&rel_time);
  while((ret=sem_trywait(sem))) {
    assert(errno==EAGAIN && "failed sync calls are not yet supported!");
    ret = _S::wait(sem, timeout);
    if(ret == ETIMEDOUT) {
      ret = -1;
      saved_err = ETIMEDOUT;
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
  SCHED_TIMER_START;
  ret = sem_post(sem);
  assert(!ret && "failed sync calls are not yet supported!");
  signal(sem);
  SCHED_TIMER_END(syncfunc::sem_post, (uint64_t)sem, (uint64_t)ret);
 
  return 0;
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
    _S::putTurn();
    sched_yield();

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
    _S::putTurn();
    sched_yield();

    struct timespec curtime;
    struct timeval curtimetv;
    gettimeofday(&curtimetv, NULL);
    curtime.tv_sec = curtimetv.tv_sec;
    curtime.tv_nsec = curtimetv.tv_usec * 1000;
    if(curtime.tv_sec > abstime->tv_sec
       || (curtime.tv_sec == abstime->tv_sec &&
           curtime.tv_nsec >= abstime->tv_nsec)) {
      ret = -1;
      saved_err = ETIMEDOUT;
      break;
    }
  }
  SCHED_TIMER_END(syncfunc::sem_timedwait, (uint64_t)sem, (uint64_t)ret);
  
  errno = saved_err;
  return ret;
}

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
    wait(mu);
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
  SCHED_TIMER_END(ins, error, syncfunc::pthread_cond_timedwait, (uint64_t)cv, (uint64_t)mu, (uint64_t) ret);
 
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
int RecorderRT<_S>::__accept(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__accept(ins, error, sockfd, cliaddr, addrlen);
  BLOCK_TIMER_END(syncfunc::accept, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__accept4(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__accept4(ins, error, sockfd, cliaddr, addrlen, flags);
  BLOCK_TIMER_END(syncfunc::accept4, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__connect(unsigned ins, int &error, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__connect(ins, error, sockfd, serv_addr, addrlen);
  BLOCK_TIMER_END(syncfunc::connect, (uint64_t) ret);
  return ret;
}

static uint64_t hash(const char *buffer, int len)
{
  uint64_t ret = 0; 
  for (int i = 0; i < len; ++i)
    ret = ret * 103 + (int) buffer[i];
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__send(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__send(ins, error, sockfd, buf, len, flags);
  uint64_t sig = hash((char*)buf, len); 
  BLOCK_TIMER_END(syncfunc::send, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__sendto(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__sendto(ins, error, sockfd, buf, len, flags, dest_addr, addrlen);
  uint64_t sig = hash((char*)buf, len); 
  BLOCK_TIMER_END(syncfunc::sendto, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__sendmsg(unsigned ins, int &error, int sockfd, const struct msghdr *msg, int flags)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__sendmsg(ins, error, sockfd, msg, flags);
  uint64_t sig = hash((char*)msg, sizeof(struct msghdr)); 
  BLOCK_TIMER_END(syncfunc::sendmsg, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recv(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags)
{
  BLOCK_TIMER_START;
  ssize_t ret = Runtime::__recv(ins, error, sockfd, buf, len, flags);
  uint64_t sig = hash((char*)buf, len); 
  BLOCK_TIMER_END(syncfunc::recv, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recvfrom(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  BLOCK_TIMER_START;
  ssize_t ret = Runtime::__recvfrom(ins, error, sockfd, buf, len, flags, src_addr, addrlen);
  uint64_t sig = hash((char*)buf, len); 
  BLOCK_TIMER_END(syncfunc::recvfrom, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recvmsg(unsigned ins, int &error, int sockfd, struct msghdr *msg, int flags)
{
  BLOCK_TIMER_START;
  ssize_t ret = Runtime::__recvmsg(ins, error, sockfd, msg, flags);
  uint64_t sig = hash((char*)msg, sizeof(struct msghdr)); 
  BLOCK_TIMER_END(syncfunc::recvmsg, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__read(unsigned ins, int &error, int fd, void *buf, size_t count)
{
  BLOCK_TIMER_START;
  ssize_t ret = Runtime::__read(ins, error, fd, buf, count);
  uint64_t sig = hash((char*)buf, count); 
  BLOCK_TIMER_END(syncfunc::read, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__write(unsigned ins, int &error, int fd, const void *buf, size_t count)
{
  BLOCK_TIMER_START;
  ssize_t ret = Runtime::__write(ins, error, fd, buf, count);
  uint64_t sig = hash((char*)buf, count); 
  BLOCK_TIMER_END(syncfunc::write, (uint64_t) sig, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__select(unsigned ins, int &error, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__select(ins, error, nfds, readfds, writefds, exceptfds, timeout);
  BLOCK_TIMER_END(syncfunc::select, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__epoll_wait(unsigned ins, int &error, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__epoll_wait(ins, error, epfd, events, maxevents, timeout);
  BLOCK_TIMER_END(syncfunc::epoll_wait, (uint64_t) ret);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__sigwait(unsigned ins, int &error, const sigset_t *set, int *sig)
{
  BLOCK_TIMER_START;
  int ret = Runtime::__sigwait(ins, error, set, sig);
  BLOCK_TIMER_END(syncfunc::sigwait, (uint64_t) ret);
  return ret;
}

template <typename _S>
char *RecorderRT<_S>::__fgets(unsigned ins, int &error, char *s, int size, FILE *stream)
{
  BLOCK_TIMER_START;
  char * ret = Runtime::__fgets(ins, error, s, size, stream);
  BLOCK_TIMER_END(syncfunc::sigwait, (uint64_t) ret);
  return ret;
}

// TODO: right now we treat sleep functions just as a turn; should convert
// real time to logical time
template <typename _S>
unsigned int RecorderRT<_S>::sleep(unsigned ins, int &error, unsigned int seconds)
{
  struct timespec ts = {seconds, 0};
  SCHED_TIMER_START;
  // must call _S::getTurnCount with turn held
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(&ts);
  _S::wait(NULL, timeout);
  SCHED_TIMER_END(syncfunc::sleep, (uint64_t) seconds * 1000000000);
  if (options::exec_sleep)
    ::sleep(seconds);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::usleep(unsigned ins, int &error, useconds_t usec)
{
  struct timespec ts = {0, 1000*usec};
  SCHED_TIMER_START;
  // must call _S::getTurnCount with turn held
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(&ts);
  _S::wait(NULL, timeout);
  SCHED_TIMER_END(syncfunc::usleep, (uint64_t) usec * 1000);
  if (options::exec_sleep)
    ::usleep(usec);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::nanosleep(unsigned ins, int &error, 
                              const struct timespec *req,
                              struct timespec *rem)
{
 SCHED_TIMER_START;
   // must call _S::getTurnCount with turn held
  unsigned timeout = _S::getTurnCount() + relTimeToTurn(req);
  _S::wait(NULL, timeout);
  uint64_t nsec = !req ? 0 : (req->tv_sec * 1000000000 + req->tv_nsec); 
  SCHED_TIMER_END(syncfunc::nanosleep, (uint64_t) nsec);
  if (options::exec_sleep)
    ::nanosleep(req, rem);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::__socket(unsigned ins, int &error, int domain, int type, int protocol)
{
  return Runtime::__socket(ins, error, domain, type, protocol);
}

template <typename _S>
int RecorderRT<_S>::__listen(unsigned ins, int &error, int sockfd, int backlog)
{
  return Runtime::__listen(ins, error, sockfd, backlog);
}

template <typename _S>
int RecorderRT<_S>::__shutdown(unsigned ins, int &error, int sockfd, int how)
{
  return Runtime::__shutdown(ins, error, sockfd, how);
}

template <typename _S>
int RecorderRT<_S>::__getpeername(unsigned ins, int &error, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  return Runtime::__getpeername(ins, error, sockfd, addr, addrlen);
}

template <typename _S>
int RecorderRT<_S>::__getsockopt(unsigned ins, int &error, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  return Runtime::__getsockopt(ins, error, sockfd, level, optname, optval, optlen);
}

template <typename _S>
int RecorderRT<_S>::__setsockopt(unsigned ins, int &error, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  return Runtime::__setsockopt(ins, error, sockfd, level, optname, optval, optlen);
}

template <typename _S>
int RecorderRT<_S>::__close(unsigned ins, int &error, int fd)
{
  return Runtime::__close(ins, error, fd);
}

template <>
unsigned int RecorderRT<RecordSerializer>::sleep(unsigned ins, int &error, unsigned int seconds)
{
  typedef Runtime _P;
  return _P::sleep(ins, error, seconds);
}

template <>
int RecorderRT<RecordSerializer>::usleep(unsigned ins, int &error, useconds_t usec)
{
  typedef Runtime _P;
  return _P::usleep(ins, error, usec);
}

template <>
int RecorderRT<RecordSerializer>::nanosleep(unsigned ins, int &error, 
                                            const struct timespec *req,
                                            struct timespec *rem)
{
  typedef Runtime _P;
  return _P::nanosleep(ins, error, req, rem);
}

/*
template <typename _S>
struct hostent *RecorderRT<_S>::__gethostbyname(unsigned ins, int &error, const char *name)
{
  return Runtime::__gethostbyname(ins, error, name);
}

template <typename _S>
struct hostent *RecorderRT<_S>::__gethostbyaddr(unsigned ins, int &error, const void *addr, int len, int type)
{
  return Runtime::__gethostbyaddr(ins, error, addr, len, type);
}
*/

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
