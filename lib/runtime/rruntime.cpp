
#include <errno.h>
#include "tern/runtime/record-log.h"
#include "tern/runtime/record-runtime.h"
#include "tern/runtime/record-scheduler.h"
#include "tern/options.h"
#include "helper.h"
#include <fstream>


#ifdef _DEBUG_RECORDER
#  define dprintf(fmt...) fprintf(stderr, fmt)
#else
#  define dprintf(fmt...)
#endif

#define lock() pthread_mutex_lock(&sched_mutex)
#define unlock() pthread_mutex_unlock(&sched_mutex)

using namespace std;

namespace tern {

__thread int RRuntime::tid;

static ostream &output()
{
  static ofstream ouf("rruntime.log");
  return ouf;
}

void RRuntime::block()
{
  static int blocking = options::get<int>("SCHEDULE_NETWORK");
  if (blocking)
    assert(0);
}

void RRuntime::wakeup()
{
  static int blocking = options::get<int>("SCHEDULE_NETWORK");
  if (blocking)
    assert(0);
}

int RRuntime::find_next_thread(void) {

  //  return 0 for success
  //  return -1 for a RUNNING thread
  //  return -2 for no enabled thread (deadlock)
  //  return -3 for no enabled thread (program exit)
  int old_token = token;  //  avoid infinite loop
  do {
    if (thread_status[token] == RUNNING)
      return -1;

    if (thread_status[token] == CLOSED)
    {
      incToken();
      continue;
    }

    assert(thread_status[token] == SCHEDULING);

    args_t &args = operation[token].args;
    //res_t &res = operation[token].res;

    switch (operation[token].op)
    {
      case (syncfunc::pthread_cancel):
      {
        pthread_t th = (pthread_t) args.handle_arg;
        tid_map_t::iterator it = tid_map.find(th);
        assert(it != tid_map.end() && "cancelling a non-existing thread");
        int kill = it->second;
        if (thread_status[kill] == RUNNING)
          return -1;  //  TODO we assume it only kills at sync-point
        else
          return 0;
        break;
      }

      case (syncfunc::pthread_mutex_lock):
      case (syncfunc::pthread_cond_wait_2):
      {
        void *mu = (void*)args.handle_arg;
        mutex_status_t::iterator it = mutex_status.find(mu);
        assert(it != mutex_status.end() && "acquiring an invalid mutex");
        if (it->second)  //  mutex is taken
          incToken();
        else
          return 0;
        break;
      }
      case (syncfunc::pthread_join):
      {
        tid_map_t::const_iterator it = tid_map.find((pthread_t) args.handle_arg);
        assert(it != tid_map.end() && "cannot find thread to be joined");  
        if (thread_status[tid] == CLOSED)
          return 0;
        else
          incToken(); //  this thread is not enabled, go to the next thread.
        break;
      }
      case (syncfunc::pthread_cond_wait_1): // it's a little wired here. the thread will then be running cond_wait

      case (syncfunc::pthread_create):
      case (syncfunc::pthread_cond_signal):
      case (syncfunc::pthread_cond_broadcast):
      case (syncfunc::tern_thread_begin):
      case (syncfunc::tern_thread_end):
      case (syncfunc::pthread_mutex_init):
      case (syncfunc::pthread_mutex_destroy):
      case (syncfunc::pthread_mutex_unlock):
      case (syncfunc::pthread_mutex_trylock):
      {
        return 0;
      }
      default:
        assert(0 && "unhandled operation");
    };
  } while (token != old_token);

  int deadlock = false;
  for (int i = 0; i < threadCount; ++i)
    if (i != tid && thread_status[i] != CLOSED) 
      deadlock = true;
  if (thread_status[tid] != CLOSED && operation[tid].op != syncfunc::tern_thread_end)
    deadlock = true;

  if (deadlock)
    assert(0 && "No enabled thread, it must be a deadlock"); //  TODO it could be normally terminated.
  else
    return -3;
}

void RRuntime::do_sched(void) {
  static int dmt = options::get<int>("DMT");
  if (!dmt)
    return;

  thread_status[tid] = SCHEDULING;

  //  wait for my tern
  int ret = find_next_thread();
  if (ret == -1)
    pthread_cond_wait(&thread_waitcv[tid], &sched_mutex);
  else
  if (ret == -2)
  {
    //  deadlock
  } else
  if (ret == -3)
  {
    //  exit
  } else 
  {
    assert(!ret && "unknown return value");
    if (token != tid) 
    {
      pthread_cond_signal(&thread_waitcv[token]);
      pthread_cond_wait(&thread_waitcv[tid], &sched_mutex);
    } 
  }
  assert(token == tid && "I should be the next enabled thread");
  thread_status[tid] = RUNNING;
}

void RRuntime::move_next()
{
  static int dmt = options::get<int>("DMT");
  if (!dmt)
    return;

  incToken();
  int ret = find_next_thread();
  if (ret == 0)
    pthread_cond_signal(&thread_waitcv[token]);
}

void RRuntime::progBegin(void) {
  Logger::progBegin();
}

void RRuntime::progEnd(void) {
  Logger::progEnd();
}

void RRuntime::threadBegin(void) {
  //  this part is actually of pthread_create
  pthread_mutex_lock(&pthread_mutex);
  new_tid = tid = incThreadCount();
  thread_status[tid] = RUNNING;
  tid_map[pthread_self()] = tid;
  pthread_cond_signal(&thread_create_cv);
  pthread_mutex_unlock(&pthread_mutex);

  char logFile[64];
  getLogFilename(logFile, sizeof(logFile), tid);
  Logger::threadBegin(logFile);

  lock();
  operation[tid].op = syncfunc::tern_thread_begin;
  do_sched();

  move_next();
  unlock();
}

void RRuntime::threadEnd(unsigned insid) {
  lock();
  operation[tid].op = syncfunc::tern_thread_end;
  do_sched();
  thread_status[tid] = CLOSED;
  tid_map.erase(pthread_self());

  move_next();
  unlock();
  Logger::threadEnd();
}

int RRuntime::pthreadCreate(unsigned ins, pthread_t *thread,
         pthread_attr_t *attr, void *(*thread_func)(void*), void *arg) {
  lock();

  operation[tid].op = syncfunc::pthread_create;

  pthread_mutex_lock(&pthread_mutex);

  int ret = __tern_pthread_create(thread, attr, thread_func, arg);
  assert(!ret && "failed sync calls are not yet supported!");

  pthread_cond_wait(&thread_create_cv, &pthread_mutex);

  int child = new_tid;
  assert(child >= 0 && "no new threadID is obtained");
  operation[tid].args.int_arg = child;

  pthread_mutex_unlock(&pthread_mutex);

  do_sched();

  move_next();
  unlock();

  return ret;
}

int RRuntime::pthreadJoin(unsigned ins, pthread_t th, void **rv) {
  lock();
  operation[tid].op = syncfunc::pthread_join;
  operation[tid].args.handle_arg = (void*)th;
  do_sched();
  move_next();
  unlock();
  int ret = pthread_join(th, rv);
  return ret;
}

int RRuntime::pthreadCancel(unsigned insid, pthread_t th)
{
  lock(); 
  operation[tid].op = syncfunc::pthread_cancel;
  operation[tid].args.handle_arg = (void*)th;
  do_sched();

  tid_map_t::iterator it = tid_map.find(th);
  if (it == tid_map.end())
  {
    output() << "trying to cancel a non-existing thread" << endl;
  } else
  {
    int kill = it->second;
    if (thread_status[kill] == SCHEDULING)
    {
      thread_status[kill] = CLOSED;
      //  FIXME thread doing cond_wait is not well handled here. they are SCHEDULING but waiting for another condition
      //  variable.
      pthread_cond_signal(&thread_waitcv[kill]);  //  the thread should be waiting for waitcv[kill];
    } else
    if (thread_status[kill] == CLOSED)
    {
      //  do nothing
    } else
      assert(0 && "unhandled thread status");
    output() << "thread " << tid << " is killing thread" << kill << endl;
  }

  move_next();
  unlock();
  return Runtime::pthreadCancel(insid, th);
}

int RRuntime::pthreadMutexInit(unsigned insid, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
{  
  lock();
  operation[tid].op = syncfunc::pthread_mutex_init;
  operation[tid].args.handle_arg = (void*)mutex;
  do_sched();

  assert(mutex_status.find(mutex) == mutex_status.end() && "initializing exising mutex");
  mutex_status[mutex] = 0;

  move_next();
  int ret = pthread_mutex_init(mutex, mutexattr);
  unlock();

  assert(!ret && "should succeed");
  return ret;
}

int RRuntime::pthreadMutexDestroy(unsigned insid, pthread_mutex_t *mutex)
{
  lock();
  operation[tid].op = syncfunc::pthread_mutex_destroy;
  operation[tid].args.handle_arg = (void*)mutex;
  do_sched();

  assert(mutex_status.find(mutex) != mutex_status.end() && "initializing exising mutex");
  mutex_status.erase(mutex);

  move_next();
  int ret = pthread_mutex_destroy(mutex);
  unlock();

  assert(!ret && "should succeed");
  return ret;
}

int RRuntime::pthreadMutexLock(unsigned ins, pthread_mutex_t *mu) {
  lock();
  operation[tid].op = syncfunc::pthread_mutex_lock;
  operation[tid].args.handle_arg = (void*)mu;
  do_sched();
  move_next();
  unlock();

  int ret = pthread_mutex_lock(mu);
  assert(!ret && "should succeed");
  return ret;
}


int RRuntime::pthreadMutexTryLock(unsigned ins, pthread_mutex_t *mu) {
  lock();
  operation[tid].op = syncfunc::pthread_mutex_trylock;
  operation[tid].args.handle_arg = (void*)mu;
  do_sched();
  move_next();
  int ret = pthread_mutex_trylock(mu);
  unlock();

  return ret;
}

int RRuntime::pthreadMutexTimedLock(unsigned ins, pthread_mutex_t *mu,
                                                const struct timespec *abstime) {
  // FIXME: treat timed-lock as just lock
  return pthreadMutexLock(ins, mu);
}

int RRuntime::pthreadMutexUnlock(unsigned ins, pthread_mutex_t *mu){
  lock();
  operation[tid].op = syncfunc::pthread_mutex_unlock;
  operation[tid].args.handle_arg = (void*)mu;
  do_sched();
  move_next();
  int ret = pthread_mutex_unlock(mu);
  unlock();

  return ret;
}

int RRuntime::pthreadBarrierInit(unsigned ins, pthread_barrier_t *barrier,
                                       unsigned count) {
  assert(0 && "not implemented");
  return -1;
}


int RRuntime::pthreadBarrierWait(unsigned ins,
                                       pthread_barrier_t *barrier) {
  assert(0 && "not implemented");
  return -1;
}

int RRuntime::pthreadBarrierDestroy(unsigned ins,
                                          pthread_barrier_t *barrier) {
  assert(0 && "not implemented");
  return -1;
}

int RRuntime::pthreadCondWait(unsigned ins,
                                    pthread_cond_t *cv, pthread_mutex_t *mu){
  lock();
  operation[tid].op = syncfunc::pthread_cond_wait_1;
  operation[tid].args.mutex_cond_arg.mutex = (void*)mu;
  operation[tid].args.mutex_cond_arg.cv = (void*)cv;
  do_sched();
  move_next();
  pthread_mutex_unlock(mu);
  pthread_cond_wait(cv, &sched_mutex);
  unlock();

  lock();
  operation[tid].op = syncfunc::pthread_cond_wait_2;
  operation[tid].args.handle_arg = (void*)mu;
  do_sched();
  move_next();
  unlock();
  pthread_mutex_lock(mu);

  return 0;
}

int RRuntime::pthreadCondTimedWait(unsigned ins,
    pthread_cond_t *cv, pthread_mutex_t *mu, const struct timespec *abstime){
  //  TODO write a timed version.
  return pthreadCondWait(ins, cv, mu);
  //assert(0 && "not implemented");
  //return -1;

}

int RRuntime::pthreadCondSignal(unsigned ins, pthread_cond_t *cv){
  lock();
  operation[tid].op = syncfunc::pthread_cond_signal;
  operation[tid].args.handle_arg = (void*)cv;
  do_sched();
  move_next();
  int ret = pthread_cond_signal(cv);  //  it's actually protected by sched_mutex
  //  if multiple threads are waiting for the same cond 
  //  variable, the result is non-deterministic. 
  unlock();
  return ret;
}

int RRuntime::pthreadCondBroadcast(unsigned ins, pthread_cond_t*cv){
  lock();
  operation[tid].op = syncfunc::pthread_cond_broadcast;
  operation[tid].args.handle_arg = (void*)cv;
  do_sched();
  move_next();
  int ret = pthread_cond_broadcast(cv);  //  it's actually protected by sched_mutex
  unlock();
  return ret;
}

int RRuntime::semWait(unsigned ins, sem_t *sem) {
  assert(0 && "not implemented");
  return -1;
}

int RRuntime::semTryWait(unsigned ins, sem_t *sem) {
  assert(0 && "not implemented");
  return -1;
}

int RRuntime::semTimedWait(unsigned ins, sem_t *sem,
                                     const struct timespec *abstime) {
  assert(0 && "not implemented");
  return -1;
}

int RRuntime::semPost(unsigned ins, sem_t *sem){
  assert(0 && "not implemented");
  return -1;
}

void RRuntime::symbolic(unsigned insid, void *addr,
                              int nbyte, const char *name){
  return ;
}

int RRuntime::__socket(unsigned ins, int domain, int type, int protocol)
{
  return Runtime::__socket(ins, domain, type, protocol);
}

int RRuntime::__listen(unsigned ins, int sockfd, int backlog)
{
  return Runtime::__listen(ins, sockfd, backlog);
}

int RRuntime::__accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  block();
  int ret = Runtime::__accept(ins, sockfd, cliaddr, addrlen);
  wakeup();
  return ret;
}

int RRuntime::__connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  block();
  int ret = Runtime::__connect(ins, sockfd, serv_addr, addrlen);
  wakeup();
  return ret;
}

/*
struct hostent *RRuntime::__gethostbyname(unsigned ins, const char *name)
{
  return Runtime::__gethostbyname(ins, name);
}

struct hostent *RRuntime::__gethostbyaddr(unsigned ins, const void *addr, int len, int type)
{
  return Runtime::__gethostbyaddr(ins, addr, len, type);
}
*/

ssize_t RRuntime::__send(unsigned ins, int sockfd, const void *buf, size_t len, int flags)
{
  return Runtime::__send(ins, sockfd, buf, len, flags);
}

ssize_t RRuntime::__sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  return Runtime::__sendto(ins, sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t RRuntime::__sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags)
{
  return Runtime::__sendmsg(ins, sockfd, msg, flags);
}

ssize_t RRuntime::__recv(unsigned ins, int sockfd, void *buf, size_t len, int flags)
{
  block();
  ssize_t ret = Runtime::__recv(ins, sockfd, buf, len, flags);
  wakeup();
  return ret;
}

ssize_t RRuntime::__recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  block();
  ssize_t ret = Runtime::__recvfrom(ins, sockfd, buf, len, flags, src_addr, addrlen);
  wakeup();
  return ret;
}

ssize_t RRuntime::__recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags)
{
  block();
  ssize_t ret = Runtime::__recvmsg(ins, sockfd, msg, flags);
  wakeup();
  return ret;
}

int RRuntime::__shutdown(unsigned ins, int sockfd, int how)
{
  return Runtime::__shutdown(ins, sockfd, how);
}

int RRuntime::__getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  return Runtime::__getpeername(ins, sockfd, addr, addrlen);
}
  
int RRuntime::__getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  return Runtime::__getsockopt(ins, sockfd, level, optname, optval, optlen);
}

int RRuntime::__setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  return Runtime::__setsockopt(ins, sockfd, level, optname, optval, optlen);
}

int RRuntime::__close(unsigned ins, int fd)
{
  return Runtime::__close(ins, fd);
}

ssize_t RRuntime::__read(unsigned ins, int fd, void *buf, size_t count)
{
  block();
  ssize_t ret = Runtime::__read(ins, fd, buf, count);
  wakeup();
  return ret;
}

ssize_t RRuntime::__write(unsigned ins, int fd, const void *buf, size_t count)
{
  return Runtime::__write(ins, fd, buf, count);
}

int RRuntime::__select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  block();
  int ret = Runtime::__select(ins, nfds, readfds, writefds, exceptfds, timeout);
  wakeup();
  return ret;
}

} // namespace tern





