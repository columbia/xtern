/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <fcntl.h>
#include <string>

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

#ifdef XTERN_PLUS_DBUG
#include "dbug/interposition/interpose.h"
//xxxxxx
#endif

using namespace tern;

Runtime *Runtime::the = NULL;

int __thread TidMap::self_tid  = -1;

#ifdef XTERN_PLUS_DBUG
#include <dlfcn.h>
using namespace std;
void *Runtime::resolveDbugFunc(const char *func_name) {
  void * handle;
  void * ret;

  std::string libDbugPath = getenv("SMT_MC_ROOT");
  libDbugPath += "/mc-tools/dbug/install/lib/libdbug.so";
  if(!(handle=dlopen(libDbugPath.c_str(), RTLD_LAZY))) {
    perror("resolveDbugFunc dlopen");
    abort();
  }

  ret = dlsym(handle, func_name);

  if(dlerror()) {
    perror("resolveDbugFunc dlsym");
    abort();
  }

  dlclose(handle);
  return ret;
}
#endif

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
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, int, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("socket");
  ret = orig_func(domain, type, protocol);
#else
  ret = socket(domain, type, protocol);
#endif
  error = errno;
  return ret;
}

int Runtime::__listen(unsigned ins, int &error, int sockfd, int backlog)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("listen");
  ret = orig_func(sockfd, backlog);
#else
  ret = listen(sockfd, backlog);
#endif
  error = errno;
  return ret;
}

static bool sock_nonblock (int fd)
{
#ifndef __WIN32__
  return fcntl (fd, F_SETFL, O_NONBLOCK) >= 0;
#else
  u_long a = 1;
  return ioctlsocket (fd, FIONBIO, &a) >= 0;
#endif
}

int Runtime::__accept(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int , struct sockaddr *, socklen_t *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("accept");
  ret = orig_func(sockfd, cliaddr, addrlen);
#else
  ret = accept(sockfd, cliaddr, addrlen);
  if (options::non_block_recv)
    assert(sock_nonblock(sockfd));
#endif
  error = errno;
  return ret;
}

int Runtime::__accept4(unsigned ins, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int , struct sockaddr *, socklen_t *, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("accept4");
  ret = orig_func(sockfd, cliaddr, addrlen, flags);
#else
  ret = accept4(sockfd, cliaddr, addrlen, flags);
#endif
  error = errno;
  return ret;
}

int Runtime::__connect(unsigned ins, int &error, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int , const struct sockaddr *, socklen_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("connect");
  ret = orig_func(sockfd, serv_addr, addrlen);
#else
  int ret = connect(sockfd, serv_addr, addrlen);
  if (options::non_block_recv)
    assert(sock_nonblock(sockfd));
#endif
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
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int, const void*, size_t, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("send");
  ret = orig_func(sockfd, buf, len, flags);
#else
  ret = send(sockfd, buf, len, flags);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__sendto(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int , const void *, size_t , int , const struct sockaddr *, socklen_t );
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sendto");
  ret = orig_func(sockfd,buf,len,flags,dest_addr,addrlen);
#else
  ret = sendto(sockfd,buf,len,flags,dest_addr,addrlen);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__sendmsg(unsigned ins, int &error, int sockfd, const struct msghdr *msg, int flags)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int , const struct msghdr *, int );
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sendmsg");
  ret = orig_func(sockfd,msg,flags);
#else
  ret = sendmsg(sockfd,msg,flags);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__recv(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int, void*, size_t, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("recv");
  ret = orig_func(sockfd, buf, len, flags);
#else
  ret = 0;
  int try_count = 0;
  timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 1000 * 1000 * 100; // 10 ms;
  while ((int) ret < (int) len && try_count < 10)
  {
    ssize_t sr = recv(sockfd, (char*)buf + ret, len - ret, flags);

    if (sr >= 0) 
      ret += sr;
    else if (ret == 0)
      ret = -1;

    if (sr < 0 || !options::non_block_recv) // it's the end of a package
      break;

    //fprintf(stderr, "sr = %d\n", (int) sr);

    if (!sr) {
      ::nanosleep(&ts, NULL);
      ++try_count;
    }
  }
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__recvfrom(unsigned ins, int &error, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int , void *, size_t , int , struct 
sockaddr *, socklen_t *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("recvfrom");
  ret = orig_func(sockfd,buf,len,flags,src_addr,addrlen);
#else
  ret = recvfrom(sockfd,buf,len,flags,src_addr,addrlen);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__recvmsg(unsigned ins, int &error, int sockfd, struct msghdr *msg, int flags)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int , struct msghdr *, int );
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("recvmsg");
  ret = orig_func(sockfd,msg,flags);
#else
  ret = recvmsg(sockfd,msg,flags);
#endif
  error = errno;
  return ret;
}

int Runtime::__shutdown(unsigned ins, int &error, int sockfd, int how)
{
  errno = error;
  int ret = shutdown(sockfd,how);
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
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int );
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("close");
  ret = orig_func(fd);
#else
  ret = close(fd);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__read(unsigned ins, int &error, int fd, void *buf, size_t count)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  // TBD: HOW DID XTERN HANDLE THIS?
  typedef ssize_t (*orig_func_type)(int, void*, size_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("read");
  ret = orig_func(fd, buf, count);
#else
  ret = read(fd, buf, count);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__write(unsigned ins, int &error, int fd, const void *buf, size_t count)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int, const void*, size_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("write");
  ret = orig_func(fd, buf, count);
#else
  ret = write(fd, buf, count);
#endif
  error = errno;
  return ret;
}

int Runtime::__select(unsigned ins, int &error, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, fd_set*, fd_set *, fd_set*, struct 
timeval*);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("select");
  ret = orig_func(nfds, readfds, writefds, exceptfds, timeout);
#else
  ret = select(nfds, readfds, writefds, exceptfds, timeout);
#endif
  error = errno;
  return ret;
}

int Runtime::__sigwait(unsigned ins, int &error, const sigset_t *set, int *sig)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(const sigset_t *, int*);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sigwait");
  ret = orig_func(set, sig);
#else
  ret = sigwait(set, sig);
#endif
  error = errno;
  return ret;
}

int Runtime::__epoll_wait(unsigned ins, int &error, int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int , struct epoll_event *, int , int );
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("epoll_wait");
  ret = orig_func(epfd,events,maxevents,timeout);
#else
  ret = epoll_wait(epfd,events,maxevents,timeout);
#endif
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
  char* ret;
#ifdef XTERN_PLUS_DBUG
  typedef char* (*orig_func_type)(char *,int ,FILE*);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("fgets");
  ret = orig_func(s, size, stream);
#else
  ret = fgets(s, size, stream);
#endif
  error = errno;
  return ret;
}

/* Do not need to involve dbug tool here, since fork/execv
  is handled in a "within process" way by xtern. */
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
  pid_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef pid_t (*orig_func_type)(int *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("wait");
  ret = orig_func(status);
#else
  ret = wait(status);
#endif
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


