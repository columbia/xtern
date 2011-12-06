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

#include <errno.h>
#include "tern/runtime/record-log.h"
#include "tern/runtime/record-runtime.h"
#include "tern/runtime/record-scheduler.h"
#include "tern/options.h"
#include "helper.h"
#include <fstream>

//#define _DEBUG_RECORDER

#ifdef _DEBUG_RECORDER
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define dprintf(fmt...)
#endif

using namespace std;

namespace {
// make sure templates are instantiated
//tern::RecorderRT<tern::FCFSScheduler> unused_fcfsRT;
//tern::RecorderRT<tern::RRSchedulerCV> unused_rrcvRT;
}

namespace tern {

ostream &output()
{
  static ofstream ouf("instr.log");
  return ouf;
}

void InstallRuntime() { 
  std::string rt = options::get<std::string>("SCHEDULE_TYPE");
  if (rt == "RR")
    Runtime::the = new RecorderRT<RRSchedulerCV>;
  if (rt == "FCFS")
    Runtime::the = new RecorderRT<FCFSScheduler>;

  if (Runtime::the == NULL)
  {
    fprintf(stderr, "warning: runtime scheduler is not specified (RR or FCfS)\n\n\
      Set to RR by default.\n");
    fflush(stderr);
    Runtime::the = new RecorderRT<RRSchedulerCV>;
  }
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

  char logFile[64];

  sem_wait(&thread_create_sem);

  _S::threadBegin(th);
  getLogFilename(logFile, sizeof(logFile), _S::self());
  Logger::threadBegin(logFile);
  nturn = _S::incTurnCount();
  _S::putTurn();

  Logger::the->logSync(INVALID_INSID, syncfunc::tern_thread_begin,
                       nturn, true, (uint64_t)th);
}

template <typename _S>
void RecorderRT<_S>::threadEnd(unsigned insid) {
  unsigned nturn;
  pthread_t th = pthread_self();

  _S::getTurn();
  nturn = _S::incTurnCount();
  _S::threadEnd(pthread_self());

  Logger::the->logSync(insid, syncfunc::tern_thread_end,
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
  nturn = _S::incTurnCount();

//  fprintf(ouf, "pthreadCreate %d %d %d\n", nturn, (int) _S::self(), ret);
  output() << "pthreadCreate";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << ret;
  output() << endl;

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
  nturn = _S::incTurnCount();
  ret = pthread_join(th, rv);
  assert(!ret && "failed sync calls are not yet supported!");

  output() << "pthreadJoin";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << endl;

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
  nturn = _S::incTurnCount();

  output() << "pthread_mutex_lock";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << mu;
  output() << endl;

  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_mutex_lock,
                       nturn, true, (uint64_t)mu);
  return 0;
}


/// instead of looping to get lock as how we implement the regular lock(),
/// here just trylock once and return.  this preserves the semantics of
/// trylock().  Note that we only log a trylock() operation when the lock
/// is actually acquired.
template <typename _S>
int RecorderRT<_S>::pthreadMutexTryLock(unsigned ins, pthread_mutex_t *mu) {
  unsigned nturn, ret;

  _S::getTurn();
  ret = pthread_mutex_trylock(mu);
  assert((!ret || ret==EBUSY)
         && "failed sync calls are not yet supported!");
  nturn = _S::incTurnCount();

  output() << "pthread_mutex_trylock";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << mu;
  output() << ' ' << ret;
  output() << endl;


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

  nturn = _S::incTurnCount();

  output() << "pthread_mutex_unlock";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << mu;
  output() << endl;


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


  int nturn = _S::incTurnCount(); //  add by Huayang, not written into Logger
  output() << "pthread_barrier_init";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << barrier;
  output() << endl;

  _S::putTurn();
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
int RecorderRT<_S>::pthreadBarrierWait(unsigned ins,
                                       pthread_barrier_t *barrier) {
  int ret, nturn1, nturn2;

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
  _S::getTurn();
  nturn1 = _S::incTurnCount();
  barrier_map::iterator bi = barriers.find(barrier);
  assert(bi!=barriers.end() && "barrier is not initialized!");
  barrier_t &b = bi->second;

  output() << "pthread_barrier_wait_first";
  output() << ' ' << nturn1;
  output() << ' ' << (int) _S::self();
  output() << ' ' << barrier;
  output() << endl;

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
  nturn2 = _S::incTurnCount();

  output() << "pthread_barrier_wait_second";
  output() << ' ' << nturn2;
  output() << ' ' << (int) _S::self();
  output() << ' ' << barrier;
  output() << endl;

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

  int nturn = _S::incTurnCount(); // add by huayang, not written into Logger
  output() << "pthread_barrier_destroy";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << barrier;
  output() << endl;

  _S::putTurn();
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
///  solution 4: lock + semaphore.  deterministic, good if sync frequency
///  low.
///
///  solution 4 optimization: lock + flag.  deterministic, good if sync
///  frequency high
///
///  solution 5: probably not worth it
///
template <typename _S>
int RecorderRT<_S>::pthreadCondWait(unsigned ins,
                                    pthread_cond_t *cv, pthread_mutex_t *mu){
  unsigned nturn1, nturn2;

  _S::getTurn();
  pthread_mutex_unlock(mu);
  nturn1 = _S::incTurnCount();

  output() << "pthread_cond_wait_first";
  output() << ' ' << nturn1;
  output() << ' ' << (int) _S::self();
  output() << ' ' << cv;
  output() << ' ' << mu;
  output() << endl;

  _S::signal(mu);
  _S::waitFirstHalf(cv);

  pthread_cond_wait(cv, _S::getLock());

  _S::getTurnNU();
  pthreadMutexLockHelper(mu);
  nturn2 = _S::incTurnCount();

  output() << "pthread_cond_wait_second";
  output() << ' ' << nturn2;
  output() << ' ' << (int) _S::self();
  output() << ' ' << cv;
  output() << ' ' << mu;
  output() << endl;

  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn1, /* before */ false, (uint64_t)cv, (uint64_t)mu);
  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn2, /* after */ true, (uint64_t)cv, (uint64_t)mu);
  return 0;
}

/// timeout based on real time is inherently nondeterministic.  three ways
/// to solve
///
/// - ignore.  drawback: changed semantics.  program may get stuck (e.g.,
///   pbzip2) if they rely on timeout to move forward.
///
/// - record timeout, and try to replay (tern approach)
///
/// - replace physical time with logical number of sync operations.  can
///   count how many times we get the turn or the turn changed hands, and
///   timeout based on this turn count.  This approach should maintain
///   original semantics if program doesn't rely on physical running time
///   of code.
///
///   optimization, if there's a deadlock (activeq == empty), we just wake
///   up timed waiters in order
///
///   any issue with this approach?
///
template <typename _S>
int RecorderRT<_S>::pthreadCondTimedWait(unsigned ins,
    pthread_cond_t *cv, pthread_mutex_t *mu, const struct timespec *abstime){

  unsigned nturn1, nturn2;

  _S::getTurn();
  pthread_mutex_unlock(mu);
  nturn1 = _S::incTurnCount();

  output() << "pthread_cond_timedwait_first";
  output() << ' ' << nturn1;
  output() << ' ' << (int) _S::self();
  output() << ' ' << cv;
  output() << ' ' << mu;
  output() << endl;


  _S::signal(mu);
  _S::waitFirstHalf(cv);

  pthread_cond_timedwait(cv, _S::getLock(), abstime);
  dprintf("abstime = %p\n", (void*)abstime);
  int ret = 0;

  // NOTE: we can't use the return value of pthread_cond_timedwait above
  // to determine whether we woke up from a timeout, as we implement
  // tern_pthread_cond_signal using pthread_cond_broadcast, which wake up
  // all waiting threads
  if(_S::isWaiting()) {
    // timed out --- which is nondeterministic
    ret = ETIMEDOUT;
    _S::signalNN(cv);
    dprintf("%d timed out from timedwait\n", _S::self());
  }

  _S::getTurnNU();
  pthreadMutexLockHelper(mu);
  nturn2 = _S::incTurnCount();

  output() << "pthread_cond_timedwait_second";
  output() << ' ' << nturn2;
  output() << ' ' << (int) _S::self();
  output() << ' ' << cv;
  output() << ' ' << mu;
  output() << endl;

  _S::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_timedwait,
                       nturn1, /* before */ false, (uint64_t)cv, (uint64_t)mu);
  Logger::the->logSync(ins, syncfunc::pthread_cond_timedwait,
                       nturn2, /* after */ true, (uint64_t)cv, (uint64_t)mu,
                       ret==ETIMEDOUT);
  return ret;
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
  nturn = _S::incTurnCount();

  output() << "pthread_cond_signal";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << cv;
  output() << endl;

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
  nturn = _S::incTurnCount();

  output() << "pthread_cond_broadcast";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << cv;
  output() << endl;

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

  nturn = _S::incTurnCount();

  output() << "pthread_sem_wait";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << sem;
  output() << endl;

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
  nturn = _S::incTurnCount();

  output() << "pthread_sem_try_wait";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << sem;
  output() << ' ' << ret;
  output() << endl;

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

  nturn = _S::incTurnCount();

  output() << "pthread_sem_post";
  output() << ' ' << nturn;
  output() << ' ' << (int) _S::self();
  output() << ' ' << sem;
  output() << endl;

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
  nturn = _S::incTurnCount();
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
template <>
int RecorderRT<FCFSScheduler>::pthreadCondWait(unsigned ins,
                pthread_cond_t *cv, pthread_mutex_t *mu){
  unsigned nturn1, nturn2;

  FCFSScheduler::getTurn();
  pthread_mutex_unlock(mu);
  nturn1 = FCFSScheduler::incTurnCount();

  pthread_cond_wait(cv, FCFSScheduler::getLock());

  pthreadMutexLockHelper(mu);
  nturn2 = FCFSScheduler::incTurnCount();
  FCFSScheduler::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn1, false, (uint64_t)cv, (uint64_t)mu);
  Logger::the->logSync(ins, syncfunc::pthread_cond_wait,
                       nturn2, true, (uint64_t)cv, (uint64_t)mu);
  return 0;
}

template <>
int RecorderRT<FCFSScheduler>::pthreadCondTimedWait(unsigned ins,
    pthread_cond_t *cv, pthread_mutex_t *mu, const struct timespec *abstime){
  unsigned nturn1, nturn2;

  FCFSScheduler::getTurn();
  pthread_mutex_unlock(mu);
  nturn1 = FCFSScheduler::incTurnCount();

  int ret = pthread_cond_timedwait(cv, FCFSScheduler::getLock(), abstime);

  pthreadMutexLockHelper(mu);
  nturn2 = FCFSScheduler::incTurnCount();
  FCFSScheduler::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_timedwait,
                       nturn1, false, (uint64_t)cv, (uint64_t)mu);
  Logger::the->logSync(ins, syncfunc::pthread_cond_timedwait,
                       nturn2, true, (uint64_t)cv, (uint64_t)mu, ret==ETIMEDOUT);
  return 0;
}


template <>
int RecorderRT<FCFSScheduler>::pthreadCondSignal(unsigned ins,
                                                 pthread_cond_t *cv){
  unsigned nturn;

  FCFSScheduler::getTurn();
  pthread_cond_signal(cv);
  nturn = FCFSScheduler::incTurnCount();
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
  nturn = FCFSScheduler::incTurnCount();
  FCFSScheduler::putTurn();

  Logger::the->logSync(ins, syncfunc::pthread_cond_broadcast,
                       nturn, true, (uint64_t)cv);
  return 0;
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
/// if in replay, t1 grabs lock first before t3, the replay will deadlock.

template <typename _S>
int RecorderRT<_S>::__socket(unsigned ins, int domain, int type, int protocol)
{
  return Runtime::__socket(ins, domain, type, protocol);
}

template <typename _S>
int RecorderRT<_S>::__listen(unsigned ins, int sockfd, int backlog)
{
  return Runtime::__listen(ins, sockfd, backlog);
}

template <typename _S>
int RecorderRT<_S>::__accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  _S::block();
  int ret = Runtime::__accept(ins, sockfd, cliaddr, addrlen);
  _S::wakeup();
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  _S::block();
  int ret = Runtime::__connect(ins, sockfd, serv_addr, addrlen);
  _S::wakeup();
  return ret;
}

/*
template <typename _S>
struct hostent *RecorderRT<_S>::__gethostbyname(unsigned ins, const char *name)
{
  return Runtime::__gethostbyname(ins, name);
}

template <typename _S>
struct hostent *RecorderRT<_S>::__gethostbyaddr(unsigned ins, const void *addr, int len, int type)
{
  return Runtime::__gethostbyaddr(ins, addr, len, type);
}
*/

template <typename _S>
ssize_t RecorderRT<_S>::__send(unsigned ins, int sockfd, const void *buf, size_t len, int flags)
{
  return Runtime::__send(ins, sockfd, buf, len, flags);
}

template <typename _S>
ssize_t RecorderRT<_S>::__sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  return Runtime::__sendto(ins, sockfd, buf, len, flags, dest_addr, addrlen);
}

template <typename _S>
ssize_t RecorderRT<_S>::__sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags)
{
  return Runtime::__sendmsg(ins, sockfd, msg, flags);
}

template <typename _S>
ssize_t RecorderRT<_S>::__recv(unsigned ins, int sockfd, void *buf, size_t len, int flags)
{
  _S::block();
  ssize_t ret = Runtime::__recv(ins, sockfd, buf, len, flags);
  _S::wakeup();
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  _S::block();
  ssize_t ret = Runtime::__recvfrom(ins, sockfd, buf, len, flags, src_addr, addrlen);
  _S::wakeup();
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags)
{
  _S::block();
  ssize_t ret = Runtime::__recvmsg(ins, sockfd, msg, flags);
  _S::wakeup();
  return ret;
}

template <typename _S>
int RecorderRT<_S>::__shutdown(unsigned ins, int sockfd, int how)
{
  return Runtime::__shutdown(ins, sockfd, how);
}

template <typename _S>
int RecorderRT<_S>::__getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  return Runtime::__getpeername(ins, sockfd, addr, addrlen);
}
  
template <typename _S>
int RecorderRT<_S>::__getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  return Runtime::__getsockopt(ins, sockfd, level, optname, optval, optlen);
}

template <typename _S>
int RecorderRT<_S>::__setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  return Runtime::__setsockopt(ins, sockfd, level, optname, optval, optlen);
}

template <typename _S>
int RecorderRT<_S>::__close(unsigned ins, int fd)
{
  return Runtime::__close(ins, fd);
}

template <typename _S>
ssize_t RecorderRT<_S>::__read(unsigned ins, int fd, void *buf, size_t count)
{
  _S::block();
  ssize_t ret = Runtime::__read(ins, fd, buf, count);
  _S::wakeup();
  return ret;
}

template <typename _S>
ssize_t RecorderRT<_S>::__write(unsigned ins, int fd, const void *buf, size_t count)
{
  return Runtime::__write(ins, fd, buf, count);
}

template <typename _S>
int RecorderRT<_S>::__select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  _S::block();
  int ret = Runtime::__select(ins, nfds, readfds, writefds, exceptfds, timeout);
  _S::wakeup();
  return ret;
}

} // namespace tern





