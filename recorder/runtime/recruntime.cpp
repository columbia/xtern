// Authors: Junfeng Yang (junfeng@cs.columbia.edu).  Refactored from
// Heming's Memoizer code

#include "log.h"
#include "recruntime.h"
#include "recscheduler.h"

using namespace std;

namespace tern {

void Runtime::install() {
  Runtime::the = new RecorderRuntime<FCFSScheduler>();
}

template <typename _Sched>
void RecorderRuntime<_Sched>::progBegin(void) {
  tern_log_begin();
}

template <typename _Sched>
void RecorderRuntime<_Sched>::progEnd(void) {
  tern_log_end();
}

template <typename _Sched>
void RecorderRuntime<_Sched>::threadBegin(void) {
  int syncid;

  sem_wait(&thread_create_sem);

  _Sched::threadBegin();
  syncid = tick();
  _Sched::putTurn();

  tern_log_thread_begin();
  //logger.log(syncfunc::thread_begin, nsync);
}

template <typename _Sched>
void RecorderRuntime<_Sched>::threadEnd() {
  int syncid;

  _Sched::getTurn();
  syncid = tick();
  _Sched::threadEnd();

  //logger.log(ins, syncfunc::thread_end, nsync);
}

/// We must assign turn tid of new thread while holding turn, or multiple
/// newly created thread could get their turn tids nondeterministically.
/// consider the nondeterminism example below if we were to assign turn
/// tid w/o holding the turn:
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
/// in a different run, t1 may run first and get turn tid 2.  To do so, we
/// use pthread_create_sem to effectively create thread in suspended mode,
/// until the parent thread applies a tid for the child.
///
template <typename _Sched>
int RecorderRuntime<_Sched>::pthreadCreate(int ins, pthread_t *thread,
         pthread_attr_t *attr, void *(*thread_func)(void*), void *arg) {
  int ret, syncid;

  _Sched::getTurn();

  ret = __tern_pthread_create(thread, attr, thread_func, arg);
  assert(!ret && "logging of failed sync calls is not yet supported!");
  _Sched::threadCreate(*thread);
  sem_post(&thread_create_sem);
  syncid = tick();

  _Sched::putTurn();

  // logger.log(ins, syncfunc::pthread_create, syncid, tid);

  return ret;
}

template <typename _Sched>
int RecorderRuntime<_Sched>::pthreadJoin(int ins, pthread_t th, void **rv) {
  int tid, ret, syncid;

  for(;;) {
    _Sched::getTurn();
    if(_Sched::validPthreadTid(th))
      _Sched::wait((void*)tid);
    else
      break;
  }
  syncid = tick();
  ret = pthread_join(th, rv);
  assert(!ret && "logging of failed sync calls is not yet supported!");

  _Sched::putTurn();

  //logger.log(ins, syncfunc::pthread_join, syncid, tid);
  return ret;
}


template <typename _Sched>
void RecorderRuntime<_Sched>::pthreadMutexLockHelper(pthread_mutex_t *mu) {
  int ret;
  for(;;) {
    ret = pthread_mutex_trylock(mu);
    if(ret == EBUSY)
      _Sched::wait(mu);
    else
      break;
    _Sched::getTurn();
  }
  assert(!ret && "logging of failed sync calls is not yet supported!");
}

template <typename _Sched>
int RecorderRuntime<_Sched>::pthreadMutexLock(int ins, pthread_mutex_t *mu) {
  int syncid;

  _Sched::getTurn();
  pthreadMutexLockHelper(mu);
  syncid = tick();
  _Sched::putTurn();

  //logger.log(ins, syncfunc::pthread_mutex_lock, syncid, tid);
  return 0;
}

template <typename _Sched>
int RecorderRuntime<_Sched>::pthreadMutexTryLock(int ins, pthread_mutex_t *mu) {
  int syncid, ret;

  _Sched::getTurn();
  ret = pthread_mutex_trylock(mu);
  assert((!ret || ret==EBUSY)
         && "logging of failed sync calls is not yet supported!");
  syncid = tick();
  _Sched::putTurn();

  //logger.log(ins, syncfunc::pthread_mutex_lock, syncid, tid);
  return ret;
}

template <typename _Sched>
int RecorderRuntime<_Sched>::pthreadMutexTimedLock(int ins, pthread_mutex_t *mu,
                                                const struct timespec *abstime) {
  // FIXME: treat timed-lock as just lock
  return pthreadMutexLock(ins, mu);
}

template <typename _Sched>
int RecorderRuntime<_Sched>::pthreadMutexUnlock(int ins, pthread_mutex_t *mu){
  int ret, syncid;

  _Sched::getTurn();

  ret = pthread_mutex_unlock(mu);
  assert(!ret && "logging of failed sync calls is not yet supported!");
  _Sched::signal(mu);

  syncid = tick();

  _Sched::putTurn();

  //logger.log(ins, syncfunc::pthread_mutex_unlock, syncid, tid);
  return 0;
}

template <typename _Sched>
void RecorderRuntime<_Sched>::symbolic(void *addr, int nbyte, const char *name){
  int syncid;

  _Sched::getTurn();
  syncid = tick();
  _Sched::putTurn();

  //logger.log(ins, syncfunc::tern_symbolic, syncid, addr, nbyte, name);
}

template <>
int RecorderRuntime<FCFSScheduler>::pthreadCondWait(int ins,
                     pthread_cond_t *cv, pthread_mutex_t *mu){
  int syncid1, syncid2;

  FCFSScheduler::getTurn();
  pthread_mutex_unlock(mu);
  syncid1 = tick();

  pthread_cond_wait(cv, FCFSScheduler::getLock());

  pthreadMutexLockHelper(mu);
  syncid2 = tick();
  FCFSScheduler::putTurn();

  //logger.log(ins, syncfunc::pthread_mutex_unlock, syncid, tid);
  return 0;
}

template <>
int RecorderRuntime<FCFSScheduler>::pthreadCondTimedWait(int ins,
      pthread_cond_t *cv, pthread_mutex_t *mu, const struct timespec *abstime){

  // FIXME: treat timed-wait as just wait
  return pthreadCondWait(ins, cv, mu);
}

template <>
int RecorderRuntime<FCFSScheduler>::pthreadCondSignal(int ins,
                                                      pthread_cond_t *cv){

  int syncid;

  FCFSScheduler::getTurn();
  pthread_cond_signal(cv);
  syncid = tick();
  FCFSScheduler::putTurn();

  //logger.log(ins, syncfunc::pthread_mutex_unlock, syncid, tid);
  return 0;
}

template <>
int RecorderRuntime<FCFSScheduler>::pthreadCondBroadcast(int ins,
                                                         pthread_cond_t*cv){
  int syncid;

  FCFSScheduler::getTurn();
  pthread_cond_broadcast(cv);
  syncid = tick();
  FCFSScheduler::putTurn();

  //logger.log(ins, syncfunc::pthread_mutex_unlock, syncid, tid);
  return 0;
}

} // namespace tern



/*
  Need to clean this up

  The issues with pthread_cond_wait()

  ** First issue: deadlock. Normally, we'd want to do

  getTurn();
  pthread_cond_wait(&cv, &mu);
  putTurn();

  However, pthread_cond_wait blocks, so we won't call putTurn(), thus
  deadlocking the entire system.  And there is (should be) no
  cond_trywait, unlike in the case of pthread_mutex_t.

  A naive solution is to putTurn before calling pthread_cond_wait

  getTurn();
  putTurn();
  pthread_cond_wait(&cv, &mu);

  However, this is nondeterministic.  Two nondeterminism scenarios:

  1: race with a pthread_mutex_lock() (or trylock()).  Depending on the
  timing, the lock() may or may not succeed.

  getTurn();
  putTurn();
  getTurn();
  pthread_cond_wait(&cv, &mu);       pthread_mutex_lock(&mu) (or trylock())
  putTurn();

  2: race with pthread_cond_signal() (or broadcast()).  Depending on the
  timing, the signal may or may not be received.  Note the application
  code uses "naked signal" without holding the lock, which is wrong, but
  despite so, we still have to be deterministic.

  getTurn();
  putTurn();
  getTurn();
  pthread_cond_wait(&cv, &mu);       pthread_cond_signal(&cv) (or broadcast())
  putTurn();

  our solution: replace mu with the scheduler mutex

  getTurn();
  pthread_mutex_unlock(&mu);
  signal(&mu);
  putTurnWithoutUnlock(); // does not release schedulerLock
  pthread_cond_wait(&cv, &schedulerLock);
  getTurnWithoutLock();

  retry:
  ret = pthread_mutex_trylock(&mu);
  if(ret == EBUSY) {
  wait(&mu);
  getTurn();
  goto retry;
  putTurn();

  solves race 1 & 2 at the same time because getTurn has to wait until
  schedulerLock is released.

  ** Second issue: still deadlock.

  The above approach may still deadlock because the thread we
  deterministically choose to wake up may be different from the one chosen
  nondeterministically by pthread.

  consider:
  pthread_cond_wait(&cv, &schedulerLock);          pthread_cond_wait(&cv, &schedulerLock);
  getTurnWithoutLock();                            getTurnWithoutLock()
  ret = pthread_mutex_trylock(&mu);                ret = pthread_mutex_trylock(&mu);
  ...                                              ...
  putTurn();                                       putTurn();

  When a thread calls pthread_cond_signal, suppose pthread chooses to wake
  up the second thread.  However, the first thread may be the one that
  gets the turn first.  Therefore the system would deadlock.

  One fix is to replace pthread_cond_signal with pthread_cond_broadcast,
  which wakes up all threads, and then the thread that gets the turn first
  would proceed first to get mu.  However, this is wrong because the other
  threads can proceed after the first thread releases mu.  This differs
  from the cond var semantics where one signal should only wake up one
  thread.

  However, this would not be an issue for mesa-style cond var because the
  woken up thread has to re-check its condition anyway.

  also man page says:

  "In a multi-processor, it may be impossible for an implementation of
  pthread_cond_signal() to avoid the unblocking of more than one thread
  blocked on a condition variable."

  -- IEEE/The Open Group, 2003, PTHREAD_COND_BROADCAST(P)

  But, again, we have to be deterministic despite program errors.  Also,
  linux man page is different:

  !pthread_cond_signal! restarts one of the threads that are waiting on
  the condition variable |cond|. If no threads are waiting on |cond|,
  nothing happens. If several threads are waiting on |cond|, exactly one
  is restarted, but it is not specified which.

  Thus, we propose a second fix: replace cv with our own cv.

  tern_pthread_cond_wait(&cv, &mu)
  getTurn();
  pthread_mutex_unlock(&mu);
  signal(&mu);
  putTurnWithoutUnlock(); // does not release schedulerLock
  pthread_cond_wait(&cvOfThread, &schedulerLock);
  getTurnWithoutLock();

  retry:
  ret = pthread_mutex_trylock(&mu);
  if(ret == EBUSY) {
  wait(&mu);
  getTurn();
  goto retry;

  putTurn();

  tern_pthread_cond_signal(&cv)
  getTurn();
  signal(&cv) // wake up precisely the first thread with waitVar == chan
  pthread_cond_signal(&cv);
  putTurn();


  A closer look at the code shows that we're not really using the original
  conditional variable at all.  That is, no thread ever waits on the
  original cond_var (ever calls cond_wait() on the cond var).  We may as
  well skip cond_signal.  That is, we're reimplementing cond vars with our
  own queues, lock and cond vars.

  This observation motives our next design: re-implementing cond var on
  top of semaphore, so that we can get rid of the schedulerLock

  getTurn()
  sem_wait(&semOfThread);

  putTurn()
  move self to end of active q
  find sem of head of q
  sem_post(&semOfHead);

  wait()
  move self to end of wait q
  find sem of head of q
  sem_post(&sem_of_head);


  tern_pthread_cond_wait(&cv, &mu)
  getTurn();
  pthread_mutex_unlock(&mu);
  signal(&mu);
  wait(&cv);
  putTurn();

  getTurn();
  retry:
  ret = pthread_mutex_trylock(&mu);
  if(ret == EBUSY) {
  wait(&mu);
  goto retry;
  putTurn();

  tern_pthread_cond_signal(&cv)
  getTurn();
  signal(&cv) // wake up precisely the first thread with waitVar == chan
  putTurn();


  One additional optimization we can do for programs that do sync
  frequently (1000 ops > 1 second?): replace the semaphores with integer flags.

  sem_wait(semOfThread) ==>  while(flagOfThread != 1);
  sem_post(semOfHead)   ==>  flagOfHead = 1;


  Another optimization: can change the single wait queue to be multiple
  wait queues keyed by the address they wait on, therefore no need to scan
  the mixed wait queue.

  What about cond_timedwait()?

  timeout based on real time is inherently nondeterministic.  three ways to solve

  - ignore timeout.  drawback: program may get stuck if they rely on
  timeout to move forward.  e.g., changes semantics

  - replace physical time with logical number of sync operations.
  drawback: may be off too much compared to physical time, and may get
  stuck as well if total number of sync < than the sync timeout
  number.  However, can count how many times we get the turn, and use
  it to timeout, thus should maintain original semantics if program
  doesn't rely on physical running time of code.

  actually, if there's a deadlock (activeq == empty), we just wait up
  timed waiters in order

  - record timeout, and reuse schedule (tern approach)

  What about trylock()?

  instead of looping to get lock as in regular lock, just trylock once and
  return.  we only log it when the lock is actually acquired.

  Summary

  lock + cv + cond_broadcast: deterministic if use while() with
  cond_wait, same sync var state except short periods of time within
  cond_wait

  lock + cv + replace cv: deterministic, but probably not as good as
  semaphore since redundant mutex lock.  same sync var state except
  original cond vars

  lock + semaphore: deterministic, good if sync frequency low.

  lock + flag: deterministic, good if sync frequency high

  can be more aggressive and implement more stuff on our own (mutex,
  barrier, etc), but probably unnecessary.  skip sleep and barrier_wait
  help the most

  -------------------

  Instead of round-robin, can easy use a deterministic but random sequence
  using a deterministic pseodu random number generator.

  at each decision point, toss a coin to decide which one to run.

  -------------------
  break out of turn for memoization or execution

  can deadlock if program uses ad hoc sync.  while(flag);

  currently don't handle the case ...

  -------------------

  FCFS scheduler (whoever comes first run, nondeterminisic)

  semaphore and flag approaches wouldn't make much sense.

  can use lock + cv, and since we don't force threads to get turns in some
  order, getTurnWithoutLock() is just a noop, and we don't need to replace
  cond_signal with cond_broadcast.


  can we use spinlock?  probably not due to cond_wait problem

  -------------------

  Replay scheduler

  tern_pthread_cond_wait(&cv, &mu)
  getTurn();
  move iter
  putTurn()

  // okay to do it here, as we don't care when we'll be woken up, as
  // long as we enforce the order later before exiting this hook
  pthread_cond_wait(&cv, &mu);

  pthread_mutex_unlock(&mu); // unlock in case we grabbed this mutex
  // prematurelly.  consider the schedule
  me             t2          t3
  cond_wait
  lock
  signal
  lock
  lock
  unlock

  // if in replay, we grab lock first before
  // t3, we'll be in trouble

  getTurn();
  move iter
  putTurn()
  pthread_mutex_lock(&mu);  // safe to lock here since we have this
  // mutex according to the schedule, so the
  // log will not have another
  // pthread_mutex_lock record before we call
  // unlock.


  -------------------

  replay optimization: can skip synchronization operations

  - skip no synchronization operations

  - skip sleep or barrier wait operations

  - skip all synchronization operations

*/
