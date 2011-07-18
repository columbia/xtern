// Authors: Junfeng Yang (junfeng@cs.columbia.edu).  Refactored from
// Heming's Memoizer code

#include "logger.h"
#include "recruntime.h"
#include "recscheduler.h"

#ifdef _DEBUG_RECORDER
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define dprintf(fmt...)
#endif

using namespace std;

namespace {
// make sure templates are instantiated
tern::RecorderRT<tern::FCFSScheduler> unused_fcfsRT;
tern::RecorderRT<tern::RRSchedulerCV> unused_rrcvRT;
}

namespace tern {

void InstallRuntime() {
  //Runtime::the = new RecorderRT<FCFSScheduler>;
  Runtime::the = new RecorderRT<RRSchedulerCV>;
  assert(Runtime::the && "can't create RecorderRT!");
}

template <typename _S>
void RecorderRT<_S>::progBegin(void) {
  Logger::progBegin();
}

template <typename _S>
void RecorderRT<_S>::progEnd(void) {
  Logger::progEnd();
}

template <typename _S>
void RecorderRT<_S>::threadBegin(void) {
  unsigned nturn;
  pthread_t th = pthread_self();

  sem_wait(&thread_create_sem);

  _S::threadBegin(th);
  Logger::threadBegin(_S::self());
  nturn = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(INVALID_INSID, syncfunc::tern_thread_begin,
                       nturn, true, (uint64_t)th);
}

template <typename _S>
void RecorderRT<_S>::threadEnd() {
  unsigned nturn;
  pthread_t th = pthread_self();

  _S::getTurn();
  nturn = _S::getTurnCount();
  _S::threadEnd(pthread_self());

  Logger::the->logSync(INVALID_INSID, syncfunc::tern_thread_end,
                       nturn, true, (uint64_t)th);
  Logger::threadEnd();
}

/// We must assign logical tid of new thread while holding turn, or
/// multiple newly created thread could get their logical tids
/// nondeterministically.  Consider the nondeterminism example below if we
/// were to assign turn tid w/o holding the turn:
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
template <typename _S>
int RecorderRT<_S>::pthreadCreate(unsigned ins, pthread_t *thread,
         pthread_attr_t *attr, void *(*thread_func)(void*), void *arg) {
  int ret, nturn;

  _S::getTurn();

  ret = __tern_pthread_create(thread, attr, thread_func, arg);
  assert(!ret && "failed sync calls are not yet supported!");
  _S::threadCreate(*thread);
  sem_post(&thread_create_sem);
  nturn = _S::getTurnCount();

  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_create, nturn,
                       true, (uint64_t)*thread);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadJoin(unsigned ins, pthread_t th, void **rv) {
  int ret, nturn;

  for(;;) {
    _S::getTurn();
    if(!_S::isZombie(th))
      _S::wait((void*)th);
    else
      break;
  }
  nturn = _S::getTurnCount();
  ret = pthread_join(th, rv);
  assert(!ret && "failed sync calls are not yet supported!");

  _S::threadJoin(th);
  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_join, nturn,
                       true, (uint64_t)th);
  return ret;
}


template <typename _S>
void RecorderRT<_S>::pthreadMutexLockHelper(pthread_mutex_t *mu) {
  int ret;
  for(;;) {
    ret = pthread_mutex_trylock(mu);
    if(ret == EBUSY)
      _S::wait(mu);
    else
      break;
    _S::getTurn();
  }
  assert(!ret && "failed sync calls are not yet supported!");
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexLock(unsigned ins, pthread_mutex_t *mu) {
  unsigned nturn;

  _S::getTurn();
  pthreadMutexLockHelper(mu);
  nturn = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_mutex_lock,
                       nturn, true, (uint64_t)mu);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexTryLock(unsigned ins, pthread_mutex_t *mu) {
  unsigned nturn, ret;

  _S::getTurn();
  ret = pthread_mutex_trylock(mu);
  assert((!ret || ret==EBUSY)
         && "failed sync calls are not yet supported!");
  nturn = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_mutex_lock,
                       nturn, true, (uint64_t)mu);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexTimedLock(unsigned ins, pthread_mutex_t *mu,
                                                const struct timespec *abstime) {
  // FIXME: treat timed-lock as just lock
  return pthreadMutexLock(ins, mu);
}

template <typename _S>
int RecorderRT<_S>::pthreadMutexUnlock(unsigned ins, pthread_mutex_t *mu){
  int ret, nturn;

  _S::getTurn();

  ret = pthread_mutex_unlock(mu);
  assert(!ret && "failed sync calls are not yet supported!");
  _S::signal(mu);

  nturn = _S::getTurnCount();

  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_mutex_unlock,
                       nturn, true, (uint64_t)mu);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadBarrierInit(unsigned ins, pthread_barrier_t *barrier,
                                       unsigned count) {
  int ret;
  _S::getTurn();
  ret = pthread_barrier_init(barrier, NULL, count);
  assert(!ret && "failed sync calls are not yet supported!");
  assert(barriers.find(barrier) == barriers.end()
         && "barrier already initialized!");
  barriers[barrier].count = count;
  barriers[barrier].narrived = 1; // one for the last arriver
  _S::putTurn();
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadBarrierWait(unsigned ins,
                                       pthread_barrier_t *barrier) {
  int ret, nturn1, nturn2;

  _S::getTurn();
  nturn1 = _S::getTurnCount();
  barrier_map::iterator bi = barriers.find(barrier);
  assert(bi!=barriers.end() && "barrier is not initialized!");
  barrier_t &b = bi->second;
  if(b.count == b.narrived) {
    b.narrived = 1; // barrier may be reused
    _S::broadcast(barrier);
    _S::putTurn();
  } else {
    ++ b.narrived;
    _S::wait(barrier);
  }

  ret = pthread_barrier_wait(barrier);
  assert((!ret || ret==PTHREAD_BARRIER_SERIAL_THREAD)
         && "failed sync calls are not yet supported!");

  _S::getTurn();
  _S::signal(barrier);
  nturn2 = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_barrier_wait,
                       nturn1, /* before */ false, (uint64_t)barrier);
  Logger::the->logSync(ins, syncfunc::pthread_barrier_wait,
                       nturn2, /* after */ true, (uint64_t)barrier);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadBarrierDestroy(unsigned ins,
                                          pthread_barrier_t *barrier) {
  int ret;
  _S::getTurn();
  ret = pthread_barrier_destroy(barrier);
  assert(!ret && "failed sync calls are not yet supported!");
  barrier_map::iterator bi = barriers.find(barrier);
  assert(bi != barriers.end() && "barrier not initialized!");
  barriers.erase(bi);
  _S::putTurn();
  return ret;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondWait(unsigned ins,
                pthread_cond_t *cv, pthread_mutex_t *mu){
  unsigned nturn1, nturn2;

  _S::getTurn();
  pthread_mutex_unlock(mu);
  nturn1 = _S::getTurnCount();
  _S::signal(mu);
  _S::waitFirstHalf(cv);

  pthread_cond_wait(cv, _S::getLock());

  _S::getTurnNU();
  pthreadMutexLockHelper(mu);
  nturn2 = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn1, /* before */ false, (uint64_t)cv, (uint64_t)mu);
  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn2, /* after */ true, (uint64_t)cv, (uint64_t)mu);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondTimedWait(unsigned ins,
    pthread_cond_t *cv, pthread_mutex_t *mu, const struct timespec *abstime){

  // FIXME: treat timed-wait as just wait
  return pthreadCondWait(ins, cv, mu);
}

template <typename _S>
int RecorderRT<_S>::pthreadCondSignal(unsigned ins, pthread_cond_t *cv){

  int ret, nturn;

  _S::getTurnLN();
  dprintf("RecorderRT: %d: cond_signal(%p)\n", _S::self(), (void*)cv);
  // use broadcast to wake up all waiters, so that we avoid pthread
  // nondeterministically wakes up an arbitrary thread.  If this thread is
  // different from the thread deterministically chosen by tern, we'll run
  // into a deadlock
  ret = pthread_cond_broadcast(cv);
  assert(!ret && "failed sync calls are not yet supported!");
  _S::signalNN(cv);
  nturn = _S::getTurnCount();
  _S::putTurnNU();

  Logger::the->logSync(ins, syncfunc::pthread_cond_signal,
                       nturn, true, (uint64_t)cv);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::pthreadCondBroadcast(unsigned ins, pthread_cond_t*cv){
  int ret, nturn;

  _S::getTurnLN();
  dprintf("RecorderRT: %d: cond_broadcast(%p)\n", _S::self(), (void*)cv);
  ret = pthread_cond_broadcast(cv);
  assert(!ret && "failed sync calls are not yet supported!");
  _S::broadcastNN(cv);
  nturn = _S::getTurnCount();
  _S::putTurnNU();

  Logger::the->logSync(ins, syncfunc::pthread_cond_broadcast,
                       nturn, true, (uint64_t)cv);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::semWait(unsigned ins, sem_t *sem) {
  int ret, nturn;

  for(;;) {
    _S::getTurn();
    ret = sem_trywait(sem);
    if(ret == EBUSY)
      _S::wait(sem);
    else
      break;
  }

  nturn = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::sem_wait,
                       nturn, true, (uint64_t)sem);
  return 0;
}

template <typename _S>
int RecorderRT<_S>::semTryWait(unsigned ins, sem_t *sem) {
  int ret;
  unsigned nturn;

  _S::getTurn();
  ret = sem_trywait(sem);
  if(ret < 0)
    assert(errno==EAGAIN && "failed sync calls are not yet supported!");
  nturn = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::sem_trywait,
                       nturn, true, (uint64_t)sem);
  return ret;
}

template <typename _S>
int RecorderRT<_S>::semTimedWait(unsigned ins, sem_t *sem,
                                     const struct timespec *abstime) {
  // FIXME: treat timed-wait as just wait
  return semWait(ins, sem);
}

template <typename _S>
int RecorderRT<_S>::semPost(unsigned ins, sem_t *sem){
  int ret;
  unsigned nturn;

  _S::getTurn();

  ret = sem_post(sem);
  assert(!ret && "failed sync calls are not yet supported!");
  _S::signal(sem);

  nturn = _S::getTurnCount();

  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::sem_post,
                       nturn, true, (uint64_t)sem);
  return 0;
}



template <typename _S>
void RecorderRT<_S>::symbolic(unsigned insid, void *addr,
                              int nbyte, const char *name){
  unsigned nturn;

  _S::getTurn();
  nturn = _S::getTurnCount();
  _S::putTurn();

  Logger::the->logSync(insid, syncfunc::tern_symbolic,
                       nturn, (uint64_t)addr, (uint64_t)nbyte);
}


//////////////////////////////////////////////////////////////////////////
// Partially specialize RecorderRT for scheduler FCFSScheduler.  The
// FCFSScheduler does really care about the order of synchronization
// operations, as long as the log faithfully records the actual order that
// occurs.  Thus, we can simplify the implementation of pthread cond var
// methods for FCFSScheduler.

template <>
int RecorderRT<FCFSScheduler>::pthreadCondWait(unsigned ins,
                pthread_cond_t *cv, pthread_mutex_t *mu){
  unsigned nturn1, nturn2;

  FCFSScheduler::getTurn();
  pthread_mutex_unlock(mu);
  nturn1 = FCFSScheduler::getTurnCount();

  pthread_cond_wait(cv, FCFSScheduler::getLock());

  pthreadMutexLockHelper(mu);
  nturn2 = FCFSScheduler::getTurnCount();
  FCFSScheduler::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn1, false, (uint64_t)cv, (uint64_t)mu);
  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn2, true, (uint64_t)cv, (uint64_t)mu);
  return 0;
}

template <>
int RecorderRT<FCFSScheduler>::pthreadCondSignal(unsigned ins,
                                                 pthread_cond_t *cv){
  unsigned nturn;

  FCFSScheduler::getTurn();
  pthread_cond_signal(cv);
  nturn = FCFSScheduler::getTurnCount();
  FCFSScheduler::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_signal,
                       nturn, true, (uint64_t)cv);
  return 0;
}

template <>
int RecorderRT<FCFSScheduler>::pthreadCondBroadcast(unsigned ins,
                                                    pthread_cond_t*cv){
  unsigned nturn;

  FCFSScheduler::getTurn();
  pthread_cond_broadcast(cv);
  nturn = FCFSScheduler::getTurnCount();
  FCFSScheduler::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_broadcast,
                       nturn, true, (uint64_t)cv);
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

  waitFirstHalf(&cv); // put self() to waitq for &cv, but don't wait on
                      // any internal sync obj or release scheduler lock
                      // because we'll wait on the cond var in user
                      // program

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
  would proceed first to get mu.  However, this changes the semantics of
  pthread_cond_signal because all woken up threads can proceed after the
  first thread releases mu.  This differs from the cond var semantics
  where one signal should only wake up one thread.

  This may not be an issue for mesa-style cond var because the woken up
  thread has to re-check its condition anyway.

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

  barrier has a similar problem.  we want to avoid the head of queue
  block, so must call wait(ba) and give up turn before calling
  pthread_barrier_wait.  However, when the last thread arrives, we must
  wake up these waiting threads.

  the signal op has to be done within the turn; otherwise two signal ops
  from two independent barriers can be nondeterministic and add threads to
  the queues in nondeterministic order.  e..g, suppose two barriers with
  count 1.

        t0                        t1

  getTurn()
  wait(ba1);
                          getTurn()
                          wait(ba1);
  barrier_wait(ba1)
                          barrier_wait(ba1)
  signal()                signal()

  these two signal() ops can be nondeterministic

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
