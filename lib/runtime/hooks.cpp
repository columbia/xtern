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
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace tern;

// make this a weak symbol so that a runtime implementation can replace it
void __attribute((weak)) InstallRuntime() {
  assert(0&&"A Runtime must define its own InstallRuntime() to instal itself!");
}

#ifdef __cplusplus
extern "C" {
#endif

extern void *idle_thread(void*);

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

void tern_symbolic_real(unsigned ins, void *addr,
                        int nbytes, const char *name) 
{
  int error = errno;
  Runtime::the->symbolic(ins, error, addr, nbytes, name);
  errno = error;
}

int first_thread = true;

void tern_thread_begin(void) {
  int error = errno;
  // thread begins in Sys space
  Runtime::the->threadBegin();

  if (first_thread)
  {
    Space::exitSys();
    pthread_t pt;
    tern_pthread_create(0xdeadceae, &pt, NULL, idle_thread, NULL);
    Space::enterSys();
  }
  first_thread = false;

  Space::exitSys();
  errno = error;
}

void tern_thread_end(unsigned ins) {
  int error = errno;
  Space::enterSys();
  Runtime::the->threadEnd(ins);
  // thread ends in Sys space
  errno = error;
}

int tern_pthread_cancel(unsigned ins, pthread_t thread) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCancel(ins, error, thread);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_create(unsigned ins, pthread_t *thread,  const pthread_attr_t *attr,
                        void *(*thread_func)(void*), void *arg) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCreate(ins, error, thread, const_cast<pthread_attr_t*>(attr),
                                           thread_func, arg);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_join(unsigned ins, pthread_t th, void **retval) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadJoin(ins, error, th, retval);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_mutex_init(unsigned ins, pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
  int error = errno;
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
  ret = Runtime::the->pthreadMutexInit(ins, error, mutex, mutexattr);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_mutex_destroy(unsigned ins, pthread_mutex_t *mutex) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexDestroy(ins, error, mutex);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_mutex_lock(unsigned ins, pthread_mutex_t *mutex) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexLock(ins, error, mutex);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_mutex_trylock(unsigned ins, pthread_mutex_t *mutex) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexTryLock(ins, error, mutex);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_mutex_timedlock(unsigned ins, pthread_mutex_t *mutex,
                                 const struct timespec *t) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexTimedLock(ins, error, mutex, t);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_mutex_unlock(unsigned ins, pthread_mutex_t *mutex) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadMutexUnlock(ins, error, mutex);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_cond_wait(unsigned ins, pthread_cond_t *cv,pthread_mutex_t *mu){
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondWait(ins, error, cv, mu);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_cond_timedwait(unsigned ins, pthread_cond_t *cv,
                                pthread_mutex_t *mu, const struct timespec *t){
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondTimedWait(ins, error, cv, mu, t);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_cond_signal(unsigned ins, pthread_cond_t *cv) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondSignal(ins, error, cv);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_cond_broadcast(unsigned ins, pthread_cond_t *cv) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadCondBroadcast(ins, error, cv);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_barrier_init(unsigned ins, pthread_barrier_t *barrier,
                        const pthread_barrierattr_t * attr, unsigned count) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadBarrierInit(ins, error, barrier, count);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_barrier_wait(unsigned ins, pthread_barrier_t *barrier) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadBarrierWait(ins, error, barrier);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_pthread_barrier_destroy(unsigned ins, pthread_barrier_t *barrier) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->pthreadBarrierDestroy(ins, error, barrier);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_sem_wait(unsigned ins, sem_t *sem) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->semWait(ins, error, sem);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_sem_trywait(unsigned ins, sem_t *sem) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->semTryWait(ins, error, sem);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_sem_timedwait(unsigned ins, sem_t *sem, const struct timespec *t) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->semTimedWait(ins, error, sem, t);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_sem_post(unsigned ins, sem_t *sem) {
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->semPost(ins, error, sem);
  Space::exitSys();
  errno = error;
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
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__sigwait(ins, error, set, sig);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_socket(unsigned ins, int domain, int type, int protocol)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__socket(ins, error, domain, type, protocol);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_listen(unsigned ins, int sockfd, int backlog)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__listen(ins, error, sockfd, backlog);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__accept(ins, error, sockfd, cliaddr, addrlen);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_accept4(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__accept4(ins, error, sockfd, cliaddr, addrlen, flags);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__connect(ins, error, sockfd, serv_addr, addrlen);
  Space::exitSys();
  errno = error;
  return ret;
}

/*
struct hostent *tern_gethostbyname(unsigned ins, const char *name)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__gethostbyname(ins, error, name);
  Space::exitSys();
  errno = error;
  return ret;
}

struct hostent *tern_gethostbyaddr(unsigned ins, const void *addr, int len, int type)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__gethostbyaddr(ins, error, addr, len, type);
  Space::exitSys();
  errno = error;
  return ret;
}
*/
ssize_t tern_send(unsigned ins, int sockfd, const void *buf, size_t len, int flags)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__send(ins, error, sockfd, buf, len, flags);
  Space::exitSys();
  errno = error;
  return ret;
}

ssize_t tern_sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__sendto(ins, error, sockfd, buf, len, flags, dest_addr, addrlen);
  Space::exitSys();
  errno = error;
  return ret;
}

ssize_t tern_sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__sendmsg(ins, error, sockfd, msg, flags);
  Space::exitSys();
  errno = error;
  return ret;
}

ssize_t tern_recv(unsigned ins, int sockfd, void *buf, size_t len, int flags)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__recv(ins, error, sockfd, buf, len, flags);
  Space::exitSys();
  errno = error;
  return ret;
}

ssize_t tern_recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__recvfrom(ins, error, sockfd, buf, len, flags, src_addr, addrlen);
  Space::exitSys();
  errno = error;
  return ret;
}

ssize_t tern_recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__recvmsg(ins, error, sockfd, msg, flags);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_shutdown(unsigned ins, int sockfd, int how)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__shutdown(ins, error, sockfd, how);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__getpeername(ins, error, sockfd, addr, addrlen);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__getsockopt(ins, error, sockfd, level, optname, optval, optlen);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__setsockopt(ins, error, sockfd, level, optname, optval, optlen);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_close(unsigned ins, int fd)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__close(ins, error, fd);
  Space::exitSys();
  errno = error;
  return ret;
}

ssize_t tern_read(unsigned ins, int fd, void *buf, size_t count)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__read(ins, error, fd, buf, count);
  Space::exitSys();
  errno = error;
  return ret;
}

ssize_t tern_write(unsigned ins, int fd, const void *buf, size_t count)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__write(ins, error, fd, buf, count);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_epoll_wait(unsigned ins, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__epoll_wait(ins, error, epfd, events, maxevents, timeout);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->__select(ins, error, nfds, readfds, writefds, exceptfds, timeout);
  Space::exitSys();
  errno = error;
  return ret;
}

unsigned int tern_sleep(unsigned ins, unsigned int seconds)
{
  int error = errno;
  unsigned int ret;
  Space::enterSys();
  ret = Runtime::the->sleep(ins, error, seconds);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_usleep(unsigned ins, useconds_t usec)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->usleep(ins, error, usec);
  Space::exitSys();
  errno = error;
  return ret;
}

int tern_nanosleep(unsigned ins, const struct timespec *req, struct timespec *rem)
{
  int error = errno;
  int ret;
  Space::enterSys();
  ret = Runtime::the->nanosleep(ins, error, req, rem);
  Space::exitSys();
  errno = error;
  return ret;
}

char *tern_fgets(unsigned ins, char *s, int size, FILE *stream)
{
  int error = errno;
  char *ret;
  Space::enterSys();
  ret = Runtime::the->__fgets(ins, error, s, size, stream);
  Space::exitSys();
  errno = error;
  return ret;
}


/*
int tern_poll(unsigned ins, struct pollfd *fds, nfds_t nfds, int timeout)
{
  errno = error;
  return poll(fds, nfds_t nfds, timeout);
}

int tern_open(unsigned ins, const char *pathname, int flags)
{
  errno = error;
  return open(pathname, flags);
}

int tern_open(unsigned ins, const char *pathname, int flags, mode_t mode)
{
  errno = error;
  return open(pathname, flags,mode);
}

int tern_creat(unsigned ins, const char *pathname, mode_t mode)
{
  errno = error;
  return creat(pathname,mode);
}

int tern_ppoll(unsigned ins, struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts, const sigset_t *sigmask)
{
  errno = error;
  return ppoll(fds, nfds_t nfds, timeout_ts, sigmask);
}

int tern_pselect(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask)
{
  errno = error;
  return pselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

void tern_FD_CLR(unsigned ins, int fd, fd_set *set)
{
  errno = error;
  return FD_CLR(fd, set);
}

int tern_FD_ISSET(unsigned ins, int fd, fd_set *set)
{
  errno = error;
  return FD_ISSET(fd, set);
}

void tern_FD_SET(unsigned ins, int fd, fd_set *set)
{
  errno = error;
  return FD_SET(fd, set);
}

void tern_FD_ZERO(unsigned ins, fd_set *set)
{
  errno = error;
  return FD_ZERO(set);
}

int tern_sockatmark (unsigned ins, int fd)
{
  errno = error;
  return sockatmark (fd);
}

int tern_isfdtype(unsigned ins, int fd, int fdtype)
{
  errno = error;
  return isfdtype(fd, fdtype);
}

*/

#ifdef __cplusplus
}
#endif


