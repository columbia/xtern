// Authors: Junfeng Yang (junfeng@cs.columbia.edu).  Refactored from
// Heming's Memoizer code

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <list>
#include <tr1/unordered_map>
#include "log.h"
#include "runtime-c-interface.h"
#include "common/runtime/tid.h"
#include "common/runtime/helper.h"

using namespace std;

namespace tern {

struct Scheduler {

  void getTurn(void) {
    pthread_mutex_lock(&lock);
    getTurnNoLock();
    pthread_mutex_unlock(&lock);
  }

  void putTurn(void) {
    int tid = tid_manager::self();
    pthread_mutex_lock(&lock);
    assert(tid == activeq.front());
    activeq.pop_front();
    activeq.push_back(tid);
    tid = activeq.front();
    pthread_cond_signal(&waitcv[tid]);
    pthread_mutex_unlock(&lock);
  }

  /// give up turn & deterministically wait on @chan as the last thread in
  /// @waitq
  void wait(void *chan) {
    int tid = tid_manager::self();
    pthread_mutex_lock(&lock);
    waitq.push_back(tid);
    assert(waitvar[tid] && "thread already waits!");
    waitvar[tid] = chan;
    while(waitvar[tid])
      pthread_cond_wait(&waitcv[tid], &lock);
    pthread_mutex_unlock(&lock);
  }

  /// deterministically wake up the first thread waiting on @chan on the
  /// wait queue
  void signal(void *chan) {
    pthread_mutex_lock(&lock);
    signalNoLock(chan);
    pthread_mutex_unlock(&lock);
  }

  /// if any thread is waiting on our exit, wake it up, then give up turn
  /// and exit (not putting self back to the tail of activeq
  void endThread() {
    int tid = tid_manager::self();
    pthread_mutex_lock(&lock);
    assert(tid == activeq.front());
    activeq.pop_front();
    signalNoLock((void*)tid);
    pthread_mutex_unlock(&lock);
  }

  void beginThread() {
    pthread_mutex_lock(&lock);
    tids.on_thread_begin(pthread_self());
    getTurnNoLock();
    pthread_mutex_unlock(&lock);
  }

  /// called within turn held
  void createThread(int new_tid) {
    int tid = tid_manager::self();
    assert(tid == activeq.front());
    activeq.push_back(new_tid);
  }

  /// called only by the main thread when it starts
  void createMainThread(int new_tid) {
    activeq.push_back(new_tid);
  }

  pthread_mutex_t *getLock() {
    return &lock;
  }

  Scheduler() {
    pthread_mutex_init(&lock, NULL);
    for(unsigned i=0; i<tid_manager::MaxThreads; ++i) {
      pthread_cond_init(&waitcv[i], NULL);
      waitvar[i] = NULL;
    }
  }

protected:

  /// same as getTurn() but does not acquire the scheduler lock
  void getTurnNoLock(void) {
    int tid = tid_manager::self();
    while(tid != activeq.front())
      pthread_cond_wait(&waitcv[tid], &lock);
  }


  /// same as signal() but does not acquire the scheduler lock
  void signalNoLock(void *chan) {
    int tid;
    bool found = false;
    for(list<int>::iterator th=waitq.begin(), te=waitq.end();
        th!=te&&!found; ++th) {
      tid = *th;
      if(waitvar[tid] == chan) {
        waitq.erase(th);
        activeq.push_back(tid);
        found = true;
      }
    }
    if(found)
      pthread_cond_signal(&waitcv[tid]);
  }


  list<int>       activeq;
  list<int>       waitq;
  pthread_mutex_t lock;

  // TODO: can potentially create a thread-local struct for each thread if
  // it improves performance
  pthread_cond_t  waitcv[tid_manager::MaxThreads];
  void*           waitvar[tid_manager::MaxThreads];
};

template <class Scheduler>
struct RecorderRuntime {

  void prog_begin(void) {
    tern_log_begin();
  }

  void prog_end(void) {
    tern_log_end();
  }

  void thread_begin(void) {
    int syncid;

    if(tids.is_main_thread()) {
      int tid = tids.on_thread_create(pthread_self());
      scheduler.createMainThread(tid);
    } else
      sem_wait(&thread_create_sem);

    scheduler.beginThread();
    syncid = tick();
    scheduler.putTurn();

    tern_log_thread_begin();
    //logger.log(syncfunc::thread_begin, nsync);
  }

  void thread_end() {
    int syncid, tid;

    scheduler.getTurn();
    syncid = tick();
    tid = tids.on_thread_end();
    scheduler.endThread();

    //logger.log(insid, syncfunc::thread_end, nsync);
  }

  int thread_create(int insid, pthread_t *thread,  pthread_attr_t *attr,
                    void *(*thread_func)(void*), void *arg) {
    int tid, ret, syncid;

    scheduler.getTurn();

    ret = __tern_pthread_create(thread, attr, thread_func, arg);
    assert(!ret && "logging of failed sync calls is not yet supported!");
    tid = tids.on_thread_create(*thread);
    scheduler.createThread(tid);
    sem_post(&thread_create_sem);
    syncid = tick();

    scheduler.putTurn();

    // logger.log(insid, syncfunc::pthread_create, syncid, tid);

    return ret;
  }

  int thread_join(int insid, pthread_t th, void **thread_return) {
    int tid, ret, syncid;

    for(;;) {
      scheduler.getTurn();
      tid = tids.get_tern_tid(th);
      if(tid != tid_manager::InvalidTid)
        scheduler.wait((void*)tid);
      else
        break;
    }
    syncid = tick();
    ret = pthread_join(th, thread_return);
    assert(!ret && "logging of failed sync calls is not yet supported!");

    scheduler.putTurn();

    //logger.log(insid, syncfunc::pthread_join, syncid, tid);
    return ret;
  }


  int pthread_mutex_lock(pthread_mutex_t *mutex) {
    int ret, syncid;

    for(;;) {
      scheduler.getTurn();
      ret = pthread_mutex_trylock(mutex);
      if(ret == EBUSY)
        scheduler.wait(mutex);
    }
    assert(!ret && "logging of failed sync calls is not yet supported!");
    syncid = tick();

    scheduler.putTurn();

    //logger.log(insid, syncfunc::pthread_mutex_lock, syncid, tid);
    return 0;
  }

  int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    int ret, syncid;

    scheduler.getTurn();

    ret = pthread_mutex_unlock(mutex);
    assert(!ret && "logging of failed sync calls is not yet supported!");
    scheduler.signal(mutex);

    syncid = tick();

    scheduler.putTurn();

    //logger.log(insid, syncfunc::pthread_mutex_unlock, syncid, tid);
    return 0;
  }

  void symbolic(void *addr, int nbytes, const char *name) {
    int syncid;

    scheduler.getTurn();
    syncid = tick();
    scheduler.putTurn();

    //logger.log(insid, syncfunc::tern_symbolic, syncid, addr, nbytes, name);
  }

  int tick() { return nsync++; }

  //Logger logger;
  Scheduler scheduler;
  sem_t thread_create_sem;
  int   nsync;
};

}

tern::RecorderRuntime<tern::Scheduler> recorderRuntime;

void tern_prog_begin() {
  recorderRuntime.prog_begin();
}

void tern_prog_end() {
  recorderRuntime.prog_end();
}

void tern_symbolic(void *addr, int nbytes, const char *name) {
  recorderRuntime.symbolic(addr, nbytes, name);
}

void tern_thread_begin(void) {
  recorderRuntime.thread_begin();
}

void tern_thread_end() {
  recorderRuntime.thread_end();
}

int tern_pthread_mutex_lock(pthread_mutex_t *mutex) {
  return recorderRuntime.pthread_mutex_lock(mutex);
}

int tern_pthread_mutex_unlock(pthread_mutex_t *mutex) {
  return recorderRuntime.pthread_mutex_unlock(mutex);
}

int tern_pthread_create(int insid, pthread_t *thread,  pthread_attr_t *attr,
                        void *(*thread_func)(void*), void *arg) {
  return recorderRuntime.thread_create(insid, thread, attr, thread_func, arg);
}

int tern_pthread_join(int insid, pthread_t th, void **retval) {
  return recorderRuntime.thread_join(insid, th, retval);
}
