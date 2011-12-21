/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <assert.h>
#include <stdlib.h>
#include <iostream>

#include "tern/config.h"
#include "tern/hooks.h"
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

/*
extern void __tern_prog_begin(void);  //  lib/runtime/helper.cpp
extern void __tern_prog_end(void); //  lib/runtime/helper.cpp

void tern___libc_start_main(void *func_ptr, int argc, char* argv[], void *init_func,
  void *fini_func, void *stack_end)
{
  __tern_prog_begin();
  int (*real_main) (void *a, int argc, char* argv[], void *b, void *c, void *d);
  real_main = dlsym(RTLD_NEXT, "__libc_start_main"); 

  __libc_start_main(func_ptr, argc, argv, init_func, fini_func, stack_end);
  __tern_prog_end();
}
*/

#ifdef __cplusplus
extern "C" {
#endif

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

void tern_thread_begin(void) {
  Runtime::the->threadBegin();
}

void tern_thread_end(unsigned ins) {
  Runtime::the->threadEnd(ins);
}

int tern_pthread_cancel(unsigned ins, pthread_t thread) {
  return Runtime::the->pthreadCancel(ins, thread);
}

int tern_pthread_create(unsigned ins, pthread_t *thread,  const pthread_attr_t *attr,
                        void *(*thread_func)(void*), void *arg) {
  return Runtime::the->pthreadCreate(ins, thread, const_cast<pthread_attr_t*>(attr),
                                           thread_func, arg);
}

int tern_pthread_join(unsigned ins, pthread_t th, void **retval) {
  return Runtime::the->pthreadJoin(ins, th, retval);
}

int tern_pthread_mutex_init(unsigned ins, pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
  return Runtime::the->pthreadMutexInit(ins, mutex, mutexattr);
}

int tern_pthread_mutex_destroy(unsigned ins, pthread_mutex_t *mutex) {
  return Runtime::the->pthreadMutexDestroy(ins, mutex);
}

int tern_pthread_mutex_lock(unsigned ins, pthread_mutex_t *mutex) {
  return Runtime::the->pthreadMutexLock(ins, mutex);
}

int tern_pthread_mutex_trylock(unsigned ins, pthread_mutex_t *mutex) {
  return Runtime::the->pthreadMutexTryLock(ins, mutex);
}

int tern_pthread_mutex_timedlock(unsigned ins, pthread_mutex_t *mutex,
                                 const struct timespec *t) {
  return Runtime::the->pthreadMutexTimedLock(ins, mutex, t);
}

int tern_pthread_mutex_unlock(unsigned ins, pthread_mutex_t *mutex) {
  return Runtime::the->pthreadMutexUnlock(ins, mutex);
}

int tern_pthread_cond_wait(unsigned ins, pthread_cond_t *cv,pthread_mutex_t *mu){
  return Runtime::the->pthreadCondWait(ins, cv, mu);
}

int tern_pthread_cond_timedwait(unsigned ins, pthread_cond_t *cv,
                                pthread_mutex_t *mu, const struct timespec *t){
  return Runtime::the->pthreadCondTimedWait(ins, cv, mu, t);
}

int tern_pthread_cond_signal(unsigned ins, pthread_cond_t *cv) {
  return Runtime::the->pthreadCondSignal(ins, cv);
}

int tern_pthread_cond_broadcast(unsigned ins, pthread_cond_t *cv) {
  return Runtime::the->pthreadCondBroadcast(ins, cv);
}

int tern_pthread_barrier_init(unsigned ins, pthread_barrier_t *barrier,
                        const pthread_barrierattr_t * attr, unsigned count) {
  return Runtime::the->pthreadBarrierInit(ins, barrier, count);
}

int tern_pthread_barrier_wait(unsigned ins, pthread_barrier_t *barrier) {
  return Runtime::the->pthreadBarrierWait(ins, barrier);
}

int tern_pthread_barrier_destroy(unsigned ins, pthread_barrier_t *barrier) {
  return Runtime::the->pthreadBarrierDestroy(ins, barrier);
}

int tern_sem_wait(unsigned ins, sem_t *sem) {
  return Runtime::the->semWait(ins, sem);
}

int tern_sem_trywait(unsigned ins, sem_t *sem) {
  return Runtime::the->semTryWait(ins, sem);
}

int tern_sem_timedwait(unsigned ins, sem_t *sem, const struct timespec *t) {
  return Runtime::the->semTimedWait(ins, sem, t);
}

int tern_sem_post(unsigned ins, sem_t *sem) {
  return Runtime::the->semPost(ins, sem);
}

extern __thread int need_hook;

/// just a wrapper to tern_thread_end() and pthread_exit()
void tern_pthread_exit(unsigned ins, void *retval) {
  // don't call tern_thread_end() for the main thread, since we'll call
  // tern_prog_end() later (which calls tern_thread_end())
  if(Scheduler::self() != Scheduler::MainThreadTid)
  {
    printf("calling tern_thread_end\n");
    tern_thread_end(ins);
    printf("calling tern_thread_end, done\n");
  }
  need_hook = 0;
  pthread_exit(retval);
}

void tern_exit(unsigned ins, int status) {
  tern_thread_end(ins); // main thread ends
  tern_prog_end();
  exit(status);
}

int tern_sigwait(unsigned ins, const sigset_t *set, int *sig)
{
  return Runtime::the->__sigwait(ins, set, sig); 
}

int tern_socket(unsigned ins, int domain, int type, int protocol)
{
  return Runtime::the->__socket(ins, domain, type, protocol);
}

int tern_listen(unsigned ins, int sockfd, int backlog)
{
  return Runtime::the->__listen(ins, sockfd, backlog);
}

int tern_accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  return Runtime::the->__accept(ins, sockfd, cliaddr, addrlen);
}

int tern_connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  return Runtime::the->__connect(ins, sockfd, serv_addr, addrlen);
}

/*
struct hostent *tern_gethostbyname(unsigned ins, const char *name)
{
  return Runtime::the->__gethostbyname(ins, name);
}

struct hostent *tern_gethostbyaddr(unsigned ins, const void *addr, int len, int type)
{
  return Runtime::the->__gethostbyaddr(ins, addr, len, type);
}
*/
ssize_t tern_send(unsigned ins, int sockfd, const void *buf, size_t len, int flags)
{
  return Runtime::the->__send(ins, sockfd, buf, len, flags);
}

ssize_t tern_sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  return Runtime::the->__sendto(ins, sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t tern_sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags)
{
  return Runtime::the->__sendmsg(ins, sockfd, msg, flags);
}

ssize_t tern_recv(unsigned ins, int sockfd, void *buf, size_t len, int flags)
{
  return Runtime::the->__recv(ins, sockfd, buf, len, flags);
}

ssize_t tern_recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  return Runtime::the->__recvfrom(ins, sockfd, buf, len, flags, src_addr, addrlen);
}

ssize_t tern_recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags)
{
  return Runtime::the->__recvmsg(ins, sockfd, msg, flags);
}

int tern_shutdown(unsigned ins, int sockfd, int how)
{
  return Runtime::the->__shutdown(ins, sockfd, how);
}

int tern_getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  return Runtime::the->__getpeername(ins, sockfd, addr, addrlen);
}
  
int tern_getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  return Runtime::the->__getsockopt(ins, sockfd, level, optname, optval, optlen);
}

int tern_setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  return Runtime::the->__setsockopt(ins, sockfd, level, optname, optval, optlen);
}

int tern_close(unsigned ins, int fd)
{
  return Runtime::the->__close(ins, fd);
}

ssize_t tern_read(unsigned ins, int fd, void *buf, size_t count)
{
  return Runtime::the->__read(ins, fd, buf, count);
}

ssize_t tern_write(unsigned ins, int fd, const void *buf, size_t count)
{
  return Runtime::the->__write(ins, fd, buf, count);
}

int tern_epoll_wait(unsigned ins, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  return Runtime::the->__epoll_wait(ins, epfd, events, maxevents, timeout);
}

int tern_select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return Runtime::the->__select(ins, nfds, readfds, writefds, exceptfds, timeout);
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

