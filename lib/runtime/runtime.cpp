/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <assert.h>
#include <stdlib.h>
#include <iostream>

#include "tern/config.h"
#include "tern/hooks.h"
#include "tern/space.h"
#include "tern/options.h"
#include "tern/runtime/runtime.h"
#include "tern/runtime/scheduler.h"
#include "helper.h"
#include <sys/socket.h>
#include <netdb.h>

using namespace tern;

Runtime *Runtime::the = NULL;

int __thread TidMap::self_tid  = -1;

// make this a weak symbol so that a runtime implementation can replace it
void __attribute((weak)) InstallRuntime() {
  assert(0&&"A Runtime must define its own InstallRuntime() to instal itself!");
}

#ifdef __cplusplus
extern "C" {
#endif

/*
    This idle thread is created to avoid empty runq.
    In the implementation with semaphore, there's a global token that must be 
    held by some threads. In the case that all threads are executing blocking
    function calls, the global token can be held by nothing. So we create this
    idle thread to ensure there's at least one thread in the runq to hold the 
    global token.

    Another solution is to add a flag in schedule showing if it happens that 
    runq is empty and the global token is held by no one. And recover the global
    token when some thread comes back to runq from blocking function call. 
 */
void *idle_thread(void *)
{
  //tern_thread_begin();
  while (true)
    tern_sleep(0xdeadbeef, 1);
  //tern_thread_end(-1);
}

static bool prog_began = false; // sanity
void tern_prog_begin() {
  assert(!prog_began && "tern_prog_begin() already called!");
  prog_began = true;
  Runtime::the->progBegin();
}

void tern_prog_end() {
  assert(prog_began && "tern_prog_begin() not called "  \
         "or tern_prog_end() already called!");
  prog_began = false;
  Runtime::the->progEnd();
}

void tern_symbolic_real(unsigned insid, void *addr,
                        int nbytes, const char *name) {
  Runtime::the->symbolic(insid, addr, nbytes, name);
}

int first_thread = true;

void tern_thread_begin(void) {
  // thread begins in Sys space
  Runtime::the->threadBegin();

  if (first_thread)
  {
    first_thread = false;
    Space::exitSys();
    pthread_t pt;
    tern_pthread_create(0xdeadceae, &pt, NULL, idle_thread, NULL);
    Space::enterSys();
  }

  Space::exitSys();
}

void tern_thread_end(unsigned ins) {
  Space::enterSys();
  Runtime::the->threadEnd(ins);
  // thread ends in Sys space
}

int tern_pthread_cancel(unsigned ins, pthread_t thread) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCancel(ins, thread);
  Space::exitSys();
  return ret;
}

int tern_pthread_create(unsigned ins, pthread_t *thread,  const pthread_attr_t *attr,
                        void *(*thread_func)(void*), void *arg) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCreate(ins, thread, const_cast<pthread_attr_t*>(attr),
                                           thread_func, arg);
  Space::exitSys();
  return ret;
}

int tern_pthread_join(unsigned ins, pthread_t th, void **retval) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadJoin(ins, th, retval);
  Space::exitSys();
  return ret;
}

int tern_pthread_mutex_init(unsigned ins, pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
  int ret;
  Space::enterSys();
  if (options::set_mutex_errorcheck && mutexattr == NULL)
  {
//    pthread_mutexattr_t *sharedm = new pthread_mutexattr_t;
//    pthread_mutexattr_t &psharedm = *sharedm;
    pthread_mutexattr_t psharedm;
    pthread_mutexattr_init(&psharedm);
    pthread_mutexattr_settype(&psharedm, PTHREAD_MUTEX_ERRORCHECK);
    mutexattr = &psharedm;
  }
  ret = Runtime::the->pthreadMutexInit(ins, mutex, mutexattr);
  Space::exitSys();
  return ret;
}

int tern_pthread_mutex_destroy(unsigned ins, pthread_mutex_t *mutex) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexDestroy(ins, mutex);
  Space::exitSys();
  return ret;
}

int tern_pthread_mutex_lock(unsigned ins, pthread_mutex_t *mutex) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexLock(ins, mutex);
  Space::exitSys();
  return ret;
}

int tern_pthread_mutex_trylock(unsigned ins, pthread_mutex_t *mutex) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexTryLock(ins, mutex);
  Space::exitSys();
  return ret;
}

int tern_pthread_mutex_timedlock(unsigned ins, pthread_mutex_t *mutex,
                                 const struct timespec *t) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexTimedLock(ins, mutex, t);
  Space::exitSys();
  return ret;
}

int tern_pthread_mutex_unlock(unsigned ins, pthread_mutex_t *mutex) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexUnlock(ins, mutex);
  Space::exitSys();
  return ret;
}

int tern_pthread_cond_wait(unsigned ins, pthread_cond_t *cv,pthread_mutex_t *mu){
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondWait(ins, cv, mu);
  Space::exitSys();
  return ret;
}

int tern_pthread_cond_timedwait(unsigned ins, pthread_cond_t *cv,
                                pthread_mutex_t *mu, const struct timespec *t){
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondTimedWait(ins, cv, mu, t);
  Space::exitSys();
  return ret;
}

int tern_pthread_cond_signal(unsigned ins, pthread_cond_t *cv) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondSignal(ins, cv);
  Space::exitSys();
  return ret;
}

int tern_pthread_cond_broadcast(unsigned ins, pthread_cond_t *cv) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondBroadcast(ins, cv);
  Space::exitSys();
  return ret;
}

int tern_pthread_barrier_init(unsigned ins, pthread_barrier_t *barrier,
                        const pthread_barrierattr_t * attr, unsigned count) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadBarrierInit(ins, barrier, count);
  Space::exitSys();
  return ret;
}

int tern_pthread_barrier_wait(unsigned ins, pthread_barrier_t *barrier) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadBarrierWait(ins, barrier);
  Space::exitSys();
  return ret;
}

int tern_pthread_barrier_destroy(unsigned ins, pthread_barrier_t *barrier) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadBarrierDestroy(ins, barrier);
  Space::exitSys();
  return ret;
}

int tern_sem_wait(unsigned ins, sem_t *sem) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->semWait(ins, sem);
  Space::exitSys();
  return ret;
}

int tern_sem_trywait(unsigned ins, sem_t *sem) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->semTryWait(ins, sem);
  Space::exitSys();
  return ret;
}

int tern_sem_timedwait(unsigned ins, sem_t *sem, const struct timespec *t) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->semTimedWait(ins, sem, t);
  Space::exitSys();
  return ret;
}

int tern_sem_post(unsigned ins, sem_t *sem) {
  int ret;
  Space::enterSys();
  ret = Runtime::the->semPost(ins, sem);
  Space::exitSys();
  return ret;
}

/// just a wrapper to tern_thread_end() and pthread_exit()
void tern_pthread_exit(unsigned ins, void *retval) {
  // don't call tern_thread_end() for the main thread, since we'll call
  // tern_prog_end() later (which calls tern_thread_end())
  if(Scheduler::self() != Scheduler::MainThreadTid)
  {
    // printf("calling tern_thread_end\n");
    tern_thread_end(ins);
    // printf("calling tern_thread_end, done\n");
  }
  pthread_exit(retval);
}

void tern_exit(unsigned ins, int status) {
  tern_thread_end(ins); // main thread ends
  tern_prog_end();
  exit(status);
}

int tern_sigwait(unsigned ins, const sigset_t *set, int *sig)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__sigwait(ins, set, sig);
  Space::exitSys();
  return ret;
}

int tern_socket(unsigned ins, int domain, int type, int protocol)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__socket(ins, domain, type, protocol);
  Space::exitSys();
  return ret;
}

int tern_listen(unsigned ins, int sockfd, int backlog)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__listen(ins, sockfd, backlog);
  Space::exitSys();
  return ret;
}

int tern_accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__accept(ins, sockfd, cliaddr, addrlen);
  Space::exitSys();
  return ret;
}

int tern_accept4(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__accept4(ins, sockfd, cliaddr, addrlen, flags);
  Space::exitSys();
  return ret;
}

int tern_connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__connect(ins, sockfd, serv_addr, addrlen);
  Space::exitSys();
  return ret;
}

/*
struct hostent *tern_gethostbyname(unsigned ins, const char *name)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__gethostbyname(ins, name);
  Space::exitSys();
  return ret;
}

struct hostent *tern_gethostbyaddr(unsigned ins, const void *addr, int len, int type)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__gethostbyaddr(ins, addr, len, type);
  Space::exitSys();
  return ret;
}
*/
ssize_t tern_send(unsigned ins, int sockfd, const void *buf, size_t len, int flags)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__send(ins, sockfd, buf, len, flags);
  Space::exitSys();
  return ret;
}

ssize_t tern_sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__sendto(ins, sockfd, buf, len, flags, dest_addr, addrlen);
  Space::exitSys();
  return ret;
}

ssize_t tern_sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__sendmsg(ins, sockfd, msg, flags);
  Space::exitSys();
  return ret;
}

ssize_t tern_recv(unsigned ins, int sockfd, void *buf, size_t len, int flags)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__recv(ins, sockfd, buf, len, flags);
  Space::exitSys();
  return ret;
}

ssize_t tern_recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__recvfrom(ins, sockfd, buf, len, flags, src_addr, addrlen);
  Space::exitSys();
  return ret;
}

ssize_t tern_recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__recvmsg(ins, sockfd, msg, flags);
  Space::exitSys();
  return ret;
}

int tern_shutdown(unsigned ins, int sockfd, int how)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__shutdown(ins, sockfd, how);
  Space::exitSys();
  return ret;
}

int tern_getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__getpeername(ins, sockfd, addr, addrlen);
  Space::exitSys();
  return ret;
}

int tern_getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__getsockopt(ins, sockfd, level, optname, optval, optlen);
  Space::exitSys();
  return ret;
}

int tern_setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__setsockopt(ins, sockfd, level, optname, optval, optlen);
  Space::exitSys();
  return ret;
}

int tern_close(unsigned ins, int fd)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__close(ins, fd);
  Space::exitSys();
  return ret;
}

ssize_t tern_read(unsigned ins, int fd, void *buf, size_t count)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__read(ins, fd, buf, count);
  Space::exitSys();
  return ret;
}

ssize_t tern_write(unsigned ins, int fd, const void *buf, size_t count)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__write(ins, fd, buf, count);
  Space::exitSys();
  return ret;
}

int tern_epoll_wait(unsigned ins, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__epoll_wait(ins, epfd, events, maxevents, timeout);
  Space::exitSys();
  return ret;
}

int tern_select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->__select(ins, nfds, readfds, writefds, exceptfds, timeout);
  Space::exitSys();
  return ret;
}

unsigned int tern_sleep(unsigned ins, unsigned int seconds)
{
  unsigned int ret;
  Space::enterSys();
  ret = Runtime::the->sleep(ins, seconds);
  Space::exitSys();
  return ret;
}

int tern_usleep(unsigned ins, useconds_t usec)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->usleep(ins, usec);
  Space::exitSys();
  return ret;
}

int tern_nanosleep(unsigned ins, const struct timespec *req, struct timespec *rem)
{
  int ret;
  Space::enterSys();
  ret = Runtime::the->nanosleep(ins, req, rem);
  Space::exitSys();
  return ret;
}

/*
int tern_poll(unsigned ins, struct pollfd *fds, nfds_t nfds, int timeout)
{
  return poll(fds, nfds_t nfds, timeout);
}

int tern_open(unsigned ins, const char *pathname, int flags)
{
  return open(pathname, flags);
}

int tern_open(unsigned ins, const char *pathname, int flags, mode_t mode)
{
  return open(pathname, flags,mode);
}

int tern_creat(unsigned ins, const char *pathname, mode_t mode)
{
  return creat(pathname,mode);
}

int tern_ppoll(unsigned ins, struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts, const sigset_t *sigmask)
{
  return ppoll(fds, nfds_t nfds, timeout_ts, sigmask);
}

int tern_pselect(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask)
{
  return pselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

void tern_FD_CLR(unsigned ins, int fd, fd_set *set)
{
  return FD_CLR(fd, set);
}

int tern_FD_ISSET(unsigned ins, int fd, fd_set *set)
{
  return FD_ISSET(fd, set);
}

void tern_FD_SET(unsigned ins, int fd, fd_set *set)
{
  return FD_SET(fd, set);
}

void tern_FD_ZERO(unsigned ins, fd_set *set)
{
  return FD_ZERO(set);
}

int tern_sockatmark (unsigned ins, int fd)
{
  return sockatmark (fd);
}

int tern_isfdtype(unsigned ins, int fd, int fdtype)
{
  return isfdtype(fd, fdtype);
}

*/

#ifdef __cplusplus
}
#endif

int Runtime::pthreadCancel(unsigned insid, pthread_t thread)
{
  return pthread_cancel(thread);
}

int Runtime::pthreadMutexInit(unsigned insid, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
{
  return pthread_mutex_init(mutex, mutexattr);
}

int Runtime::pthreadMutexDestroy(unsigned insid, pthread_mutex_t *mutex)
{
  return pthread_mutex_destroy(mutex);
}

int Runtime::__socket(unsigned ins, int domain, int type, int protocol)
{
  return socket(domain, type, protocol);
}

int Runtime::__listen(unsigned ins, int sockfd, int backlog)
{
  return listen(sockfd, backlog);
}

int Runtime::__accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  return accept(sockfd, cliaddr, addrlen);
}

int Runtime::__accept4(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags)
{
  return accept4(sockfd, cliaddr, addrlen, flags);
}

int Runtime::__connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  return connect(sockfd, serv_addr, addrlen);
}

/*
struct hostent *Runtime::__gethostbyname(unsigned ins, const char *name)
{
  return gethostbyname(name);
}

struct hostent *Runtime::__gethostbyaddr(unsigned ins, const void *addr, int len, int type)
{
  return gethostbyaddr(addr, len, type);
}
*/

ssize_t Runtime::__send(unsigned ins, int sockfd, const void *buf, size_t len, int flags)
{
  return send(sockfd, buf, len, flags);
}

ssize_t Runtime::__sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  return sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t Runtime::__sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags)
{
  return sendmsg(sockfd, msg, flags);
}

ssize_t Runtime::__recv(unsigned ins, int sockfd, void *buf, size_t len, int flags)
{
  return recv(sockfd, buf, len, flags);
}

ssize_t Runtime::__recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  return recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

ssize_t Runtime::__recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags)
{
  return recvmsg(sockfd, msg, flags);
}

int Runtime::__shutdown(unsigned ins, int sockfd, int how)
{
  return shutdown(sockfd, how);
}

int Runtime::__getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  return getpeername(sockfd, addr, addrlen);
}

int Runtime::__getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  return getsockopt(sockfd, level, optname, optval, optlen);
}

int Runtime::__setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  return setsockopt(sockfd, level, optname, optval, optlen);
}

int Runtime::__close(unsigned ins, int fd)
{
  return close(fd);
}

ssize_t Runtime::__read(unsigned ins, int fd, void *buf, size_t count)
{
  return read(fd, buf, count);
}

ssize_t Runtime::__write(unsigned ins, int fd, const void *buf, size_t count)
{
  return write(fd, buf, count);
}

int Runtime::__select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return select(nfds, readfds, writefds, exceptfds, timeout);
}

int Runtime::__sigwait(unsigned ins, const sigset_t *set, int *sig)
{
  return sigwait(set, sig);
}

int Runtime::__epoll_wait(unsigned ins, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  return epoll_wait(epfd, events, maxevents, timeout);
}

unsigned int Runtime::sleep(unsigned ins, unsigned int seconds)
{
  return ::sleep(seconds);
}

int Runtime::usleep(unsigned ins, useconds_t usec)
{
  return ::usleep(usec);
}

int Runtime::nanosleep(unsigned ins, const struct timespec *req, struct timespec *rem)
{
  return ::nanosleep(req, rem);
}
