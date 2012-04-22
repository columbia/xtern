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
#include <sys/types.h>
#include <sys/wait.h>

using namespace tern;

Runtime *Runtime::the = NULL;

int __thread TidMap::self_tid  = -1;

int Runtime::pthreadCancel(unsigned insid, int &error, pthread_t thread)
{
  errno = error;
  int ret = pthread_cancel(thread);
  error = errno;
  return ret;
}

int Runtime::pthreadMutexInit(unsigned insid, int &error, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
{
  errno = error;
  int ret = pthread_mutex_init(mutex, mutexattr);
  error = errno;
  return ret;
}

int Runtime::pthreadMutexDestroy(unsigned insid, int &error, pthread_mutex_t *mutex)
{
  errno = error;
  int ret = pthread_mutex_destroy(mutex);
  error = errno;
  return ret;
}

int Runtime::__socket(unsigned ins, int &error, int domain, int type, int protocol)
{
  errno = error;
  int ret = socket(domain, type, protocol);
  error = errno;
  return ret;
}

int Runtime::__listen(unsigned ins, int &error, int sockfd, int backlog)
{
  errno = error;
  int ret = listen(sockfd, backlog);
  error = errno;
  return ret;
}

int Runtime::__accept(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  errno = error;
  int ret = accept(sockfd, cliaddr, addrlen);
  error = errno;
  return ret;
}

int Runtime::__accept4(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags)
{
  errno = error;
  int ret = accept4(sockfd, cliaddr, addrlen, flags);
  error = errno;
  return ret;
}

int Runtime::__connect(unsigned ins, int &error, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  errno = error;
  int ret = connect(sockfd, serv_addr, addrlen);
  error = errno;
  return ret;
}

/*
struct hostent *Runtime::__gethostbyname(unsigned ins, int &error, const char *name)
{
  errno = error;
  int ret = gethostbyname(name);
  error = errno;
  return ret;
}

struct hostent *Runtime::__gethostbyaddr(unsigned ins, int &error, const void *addr, int len, int type)
{
  errno = error;
  int ret = gethostbyaddr(addr, len, type);
  error = errno;
  return ret;
}
*/

ssize_t Runtime::__send(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags)
{
  errno = error;
  ssize_t ret = send(sockfd, buf, len, flags);
  error = errno;
  return ret;
}

ssize_t Runtime::__sendto(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  errno = error;
  ssize_t ret = sendto(sockfd, buf, len, flags, dest_addr, addrlen);
  error = errno;
  return ret;
}

ssize_t Runtime::__sendmsg(unsigned ins, int &error, int sockfd, const struct msghdr *msg, int flags)
{
  errno = error;
  ssize_t ret = sendmsg(sockfd, msg, flags);
  error = errno;
  return ret;
}

ssize_t Runtime::__recv(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags)
{
  errno = error;
  ssize_t ret = recv(sockfd, buf, len, flags);
  error = errno;
  return ret;
}

ssize_t Runtime::__recvfrom(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  errno = error;
  ssize_t ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
  error = errno;
  return ret;
}

ssize_t Runtime::__recvmsg(unsigned ins, int &error, int sockfd, struct msghdr *msg, int flags)
{
  errno = error;
  ssize_t ret = recvmsg(sockfd, msg, flags);
  error = errno;
  return ret;
}

int Runtime::__shutdown(unsigned ins, int &error, int sockfd, int how)
{
  errno = error;
  int ret = shutdown(sockfd, how);
  error = errno;
  return ret;
}

int Runtime::__getpeername(unsigned ins, int &error, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  errno = error;
  int ret = getpeername(sockfd, addr, addrlen);
  error = errno;
  return ret;
}

int Runtime::__getsockopt(unsigned ins, int &error, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  errno = error;
  int ret = getsockopt(sockfd, level, optname, optval, optlen);
  error = errno;
  return ret;
}

int Runtime::__setsockopt(unsigned ins, int &error, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  errno = error;
  int ret = setsockopt(sockfd, level, optname, optval, optlen);
  error = errno;
  return ret;
}

int Runtime::__close(unsigned ins, int &error, int fd)
{
  errno = error;
  int ret = close(fd);
  error = errno;
  return ret;
}

ssize_t Runtime::__read(unsigned ins, int &error, int fd, void *buf, size_t count)
{
  errno = error;
  ssize_t ret = read(fd, buf, count);
  error = errno;
  return ret;
}

ssize_t Runtime::__write(unsigned ins, int &error, int fd, const void *buf, size_t count)
{
  errno = error;
  ssize_t ret = write(fd, buf, count);
  error = errno;
  return ret;
}

int Runtime::__select(unsigned ins, int &error, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  errno = error;
  int ret = select(nfds, readfds, writefds, exceptfds, timeout);
  error = errno;
  return ret;
}

int Runtime::__sigwait(unsigned ins, int &error, const sigset_t *set, int *sig)
{
  errno = error;
  int ret = sigwait(set, sig);
  error = errno;
  return ret;
}

int Runtime::__epoll_wait(unsigned ins, int &error, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  errno = error;
  int ret = epoll_wait(epfd, events, maxevents, timeout);
  error = errno;
  return ret;
}

unsigned int Runtime::sleep(unsigned ins, int &error, unsigned int seconds)
{
  errno = error;
  unsigned ret = ::sleep(seconds);
  error = errno;
  return ret;
}

int Runtime::usleep(unsigned ins, int &error, useconds_t usec)
{
  errno = error;
  int ret = ::usleep(usec);
  error = errno;
  return ret;
}

char *Runtime::__fgets(unsigned ins, int &error, char *s, int size, FILE *stream)
{
  errno = error;
  char *ret = fgets(s, size, stream);
  error = errno;
  return ret;
}

pid_t Runtime::__fork(unsigned ins, int &error)
{
  errno = error;
  pid_t ret = fork();
  error = errno;
  return ret;
}

pid_t Runtime::__wait(unsigned ins, int &error, int *status)
{
  errno = error;
  pid_t ret = wait(status);
  error = errno;
  return ret;
}

int Runtime::nanosleep(unsigned ins, int &error, const struct timespec *req, struct timespec *rem)
{
  errno = error;
  int ret = ::nanosleep(req, rem);
  error = errno;
  return ret;
}

time_t Runtime::__time(unsigned ins, int &error, time_t *t)
{
  errno = error;
  time_t ret = ::time(t);
  error = errno;
  return ret;
}

int Runtime::__clock_getres(unsigned ins, int &error, clockid_t clk_id, struct timespec *res)
{
  errno = error;
  int ret = ::clock_getres(clk_id, res);
  error = errno;
  return ret;
}

int Runtime::__clock_gettime(unsigned ins, int &error, clockid_t clk_id, struct timespec *tp)
{
  errno = error;
  int ret = ::clock_gettime(clk_id, tp);
  error = errno;
  return ret;
}

int Runtime::__clock_settime(unsigned ins, int &error, clockid_t clk_id, const struct timespec *tp)
{
  errno = error;
  int ret = ::clock_settime(clk_id, tp);
  error = errno;
  return ret;
}

int Runtime::__gettimeofday(unsigned ins, int &error, struct timeval *tv, struct timezone *tz)
{
  errno = error;
  int ret = ::gettimeofday(tv, tz);
  error = errno;
  return ret;
}

int Runtime::__settimeofday(unsigned ins, int &error, const struct timeval *tv, const struct timezone *tz)
{
  errno = error;
  int ret = ::settimeofday(tv, tz);
  error = errno;
  return ret;
}

int Runtime::__pthread_rwlock_rdlock(unsigned ins, int &error, pthread_rwlock_t *rwlock) 
{ 
  error = errno; 
  int ret = ::pthread_rwlock_rdlock(rwlock); 
  errno = error; 
  return ret; 
}

int Runtime::__pthread_rwlock_tryrdlock(unsigned ins, int &error, pthread_rwlock_t *rwlock) 
{ 
  error = errno; 
  int ret = ::pthread_rwlock_tryrdlock(rwlock); 
  errno = error; 
  return ret; 
}

int Runtime::__pthread_rwlock_wrlock(unsigned ins, int &error, pthread_rwlock_t *rwlock) 
{ 
  error = errno; 
  int ret = ::pthread_rwlock_wrlock(rwlock); 
  errno = error; 
  return ret; 
}

int Runtime::__pthread_rwlock_trywrlock(unsigned ins, int &error, pthread_rwlock_t *rwlock) 
{ 
  error = errno; 
  int ret = ::pthread_rwlock_trywrlock(rwlock); 
  errno = error; 
  return ret; 
}

int Runtime::__pthread_rwlock_unlock(unsigned ins, int &error, pthread_rwlock_t *rwlock) 
{ 
  error = errno; 
  int ret = ::pthread_rwlock_unlock(rwlock); 
  errno = error; 
  return ret; 
}

int Runtime::__pthread_rwlock_destroy(unsigned ins, int &error, pthread_rwlock_t *rwlock) 
{ 
  error = errno; 
  int ret = ::pthread_rwlock_destroy(rwlock); 
  errno = error; 
  return ret; 
}

int Runtime::__pthread_rwlock_init(unsigned ins, int &error, pthread_rwlock_t *rwlock, const pthread_rwlockattr_t * attr) 
{ 
  error = errno; 
  int ret = ::pthread_rwlock_init(rwlock, attr); 
  errno = error; 
  return ret; 
}


