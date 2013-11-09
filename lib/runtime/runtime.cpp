/* Copyright (c) 2013,  Regents of the Columbia University 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other 
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Authors: Heming Cui (heming@cs.columbia.edu), Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <fcntl.h>
#include <string>
#include <poll.h>
#include <stdarg.h>

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

//#define DEBUG_RUNTIME

#ifdef DEBUG_RUNTIME
#define dprintf(fmt...) fprintf(stderr, fmt);
#else
#define dprintf(fmt...)
#endif

#ifdef XTERN_PLUS_DBUG
#include <dbug/interposition/stubs.h>
#endif

using namespace tern;

Runtime *Runtime::the = NULL;

int __thread TidMap::self_tid  = -1;

extern pthread_t idle_th;


#ifdef XTERN_PLUS_DBUG

#include <dlfcn.h>
bool __thread attachedToDbug = true;
using namespace std;

void *Runtime::resolveDbugFunc(const char *func_name) {
  static void * handle;
  void * ret;
  dprintf("Pid %d: self %u: resolveDbugFunc %s start\n", getpid(), (unsigned)pthread_self(), func_name);
  if (!handle) {
    std::string libDbugPath = getenv("SMT_MC_ROOT");
    libDbugPath += "/mc-tools/dbug/install/lib/libdbug.so";
    if(!(handle=dlopen(libDbugPath.c_str(), RTLD_LAZY))) {
      perror("resolveDbugFunc failed to open libdbug.so");
      abort();
    }
  }
  //fprintf(stderr, "resolveDbugFunc %s dlsym\n", func_name);
  ret = dlsym(handle, func_name);

  if(dlerror()) {
    perror("resolveDbugFunc failed to resolve function");
    abort();
  }
  //fprintf(stderr, "Pid %d: self %u: resolveDbugFunc %s end\n", getpid(), (unsigned)pthread_self(), func_name);
  //dlclose(handle);
  return ret;
}

void Runtime::initDbug() {
  // Just resolve a function with the dbug library, a simple method 
  // to init dbug (code involved in dbug's interpose-impl.cc). No matter
  // whether we will involve any inter-process operation at runtime,
  // this init work is a "must".
  //resolveDbugFunc("write");
  resolveDbugFunc("pthread_create");
}

void Runtime::__thread_detach() {
  dprintf("\nxtern::Runtime::__thread_detach pid %d thread self %u from dbug\n\n", getpid(), (unsigned)pthread_self());
  //assert(attachedToDbug);
  //attachedToDbug = false;
  //dbug_detach();
  typedef void (*orig_func_type)();
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("dbug_detach");
  orig_func();
}

void Runtime::__detach_barrier_end(int bar_id, int cnt) {
  assert(attachedToDbug);
  attachedToDbug = false;
  dprintf("\nxtern::Runtime::__detach_barrier_end pid %d thread self %u from dbug\n\n", getpid(), (unsigned)pthread_self());
  typedef void (*orig_func_type)(int, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("dbug_off_barrier");
  orig_func(bar_id, cnt);
}

void Runtime::__thread_waiting() {
  typedef void (*orig_func_type)();
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("dbug_thread_waiting");
  dprintf("\n\nPid %d self %u dbug_thread_waiting start.\n\n", getpid(), (unsigned)pthread_self());
  orig_func();
  dprintf("\n\nPid %d self %u dbug_thread_waiting end.\n\n", getpid(), (unsigned)pthread_self());
}

void Runtime::__thread_active(pthread_t wakenUpTid) {
  typedef void (*orig_func_type)(pthread_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("dbug_thread_active");
  dprintf("\n\nPid %d self %u dbug_thread_active start.\n\n", getpid(), (unsigned)pthread_self());
  orig_func(wakenUpTid);
  dprintf("\n\nPid %d self %u dbug_thread_active end.\n\n", getpid(), (unsigned)pthread_self());
}

#else
static bool sock_nonblock (int fd)
{
#ifndef __WIN32__
  return fcntl (fd, F_SETFL, O_NONBLOCK) >= 0;
#else
  u_long a = 1;
  return ioctlsocket (fd, FIONBIO, &a) >= 0;
#endif
}
#endif

void Runtime::__attach_self_to_dbug(const char *caller) {
#ifdef XTERN_PLUS_DBUG
  dprintf("\nxtern::Runtime::__attach_self_to_mc pid %d thread self %u to dbug\n\n", getpid(), (unsigned)pthread_self());
  //errno = error;
  if (attachedToDbug) {
    fprintf(stderr, "\nxtern::Runtime::__attach_self_to_mc pid %d thread self %u to dbug failure, caller (%s)\n\n", getpid(), (unsigned)pthread_self(), caller);
    assert(false);
  }
  attachedToDbug = true;
  //dbug_on();
  typedef void (*orig_func_type)();
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("dbug_on");
  orig_func();
#endif
}

void Runtime::__detach_self_from_dbug(const char *caller) {
#ifdef XTERN_PLUS_DBUG
  dprintf("\nxtern::Runtime::__detach_self_from_mc pid %d thread self %u from dbug\n\n", getpid(), (unsigned)pthread_self());
  //errno = error;
  assert(attachedToDbug);
  attachedToDbug = false;
  //dbug_off();
  typedef void (*orig_func_type)();
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("dbug_off");
  orig_func();
#endif
}

Runtime::Runtime() {
#ifdef XTERN_PLUS_DBUG
  initDbug();
#endif
}

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

int Runtime::__pthread_create(pthread_t *th, const pthread_attr_t *a, void *(*func)(void*), void *arg) {
  //errno = error;
  int ret;
//#if 0
#ifdef XTERN_PLUS_DBUG
  if (th != &idle_th) { // Idle thread is xtern an internal thread, we must not register it in dbug.
    __attach_self_to_dbug(__FUNCTION__);
    typedef int (*orig_func_type)(pthread_t *,const pthread_attr_t *,void *(*)(void*),void *);
    static orig_func_type orig_func;
    if (!orig_func)
      orig_func = (orig_func_type)resolveDbugFunc("pthread_create");
    ret = orig_func(th, a, func, arg);
     __detach_self_from_dbug(__FUNCTION__);
  } else {
    fprintf(stderr, "Created idle thread in Runtime.\n");
    ret = pthread_create(th, a, func, arg);
  }
#else
  ret = pthread_create(th, a, func, arg);
#endif
  //error = errno;
  return ret;
}

void Runtime::__pthread_exit(void *value_ptr) {
//#if 0
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(void *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pthread_exit");
  orig_func(value_ptr);
#else
  pthread_exit(value_ptr);
#endif
}

int Runtime::__pthread_join(pthread_t th, void **retval) {
  int ret;
#ifdef XTERN_PLUS_DBUG
  __attach_self_to_dbug(__FUNCTION__);
  typedef int (*orig_func_type)(pthread_t, void **);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pthread_join");
  ret = orig_func(th, retval);
  __detach_self_from_dbug(__FUNCTION__);
#else
  ret = pthread_join(th, retval);
#endif
  return ret;
}

int Runtime::__pthread_detach(unsigned ins, int &error, pthread_t th) {
  int ret;
  errno = error;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(pthread_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pthread_detach");
  fprintf(stderr, "Calling Runtime::___pthread_detach into dbug\n");
  ret = orig_func(th);
#else
  ret = ::pthread_detach(th);
#endif
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
  dprintf("Parrot pid %d self %u calls dbug socket(%d, %d, %d)...\n", getpid(), (unsigned)pthread_self(), domain, type, protocol);
  ret = orig_func(domain, type, protocol);
  dprintf("Parrot pid %d self %u calls dbug socket() return...\n", getpid(), (unsigned)pthread_self());
#else
  ret = socket(domain, type, protocol);
#endif
  error = errno;
  return ret;
}

int Runtime::__getsockopt(unsigned ins, int &error, int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, int, int, void *, socklen_t *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("getsockopt");
  ret = orig_func(sockfd, level, optname, optval, optlen);
#else
  ret = ::getsockopt(sockfd, level, optname, optval, optlen);
#endif
  error = errno;
  return ret;
}

int Runtime::__setsockopt(unsigned ins, int &error, int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, int, int, const void *, socklen_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("setsockopt");
  ret = orig_func(sockfd, level, optname, optval, optlen);
#else
  ret = ::setsockopt(sockfd, level, optname, optval, optlen);
#endif
  error = errno;
  return ret;
}

int Runtime::__pipe(unsigned ins, int &error, int pipefd[2])
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int pipefd[2]);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pipe");
  ret = orig_func(pipefd);
#else
  ret = ::pipe(pipefd);
#endif
  error = errno;
  return ret;
}

int Runtime::__fcntl(unsigned ins, int &error, int fd, int cmd, void *arg)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, int, ...);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("fcntl");
  dprintf("Parrot pid %d self %u calls dbug fcntl(%d, %d, %p)...\n", getpid(),
    (unsigned)pthread_self(), fd, cmd, arg);
  ret = orig_func(fd, cmd, arg);
#else
  ret = ::fcntl(fd, cmd, arg);
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
  dprintf("Parrot pid %d self %u calls dbug listen()...\n", getpid(), (unsigned)pthread_self());
  ret = orig_func(sockfd, backlog);
  dprintf("Parrot pid %d self %u calls dbug listen() return...\n", getpid(), (unsigned)pthread_self());
#else
  ret = listen(sockfd, backlog);
#endif
  error = errno;
  return ret;
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
  dprintf("Parrot pid %d self %u calls dbug accept...\n", getpid(), (unsigned)pthread_self());
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
  dprintf("Parrot pid %d self %u calls dbug connect...\n", getpid(), (unsigned)pthread_self());
  ret = orig_func(sockfd, serv_addr, addrlen);
  dprintf("Self %u: Runtime::__connect returns %d\n", (unsigned)pthread_self(), ret);
#else
  ret = connect(sockfd, serv_addr, addrlen);
  if (options::non_block_recv)
    assert(sock_nonblock(sockfd));
#endif
  error = errno;
  return ret;
}

struct hostent *Runtime::__gethostbyname(unsigned ins, int &error, const char *name)
{
  errno = error;
  struct hostent *ret;
#ifdef XTERN_PLUS_DBUG
  typedef struct hostent * (*orig_func_type)(const char *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("gethostbyname");
  ret = orig_func(name);
#else
  ret = gethostbyname(name);
#endif
  error = errno;
  return ret;
}

int Runtime::__gethostbyname_r(unsigned ins, int &error, const char *name,
  struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
  errno = error;
  int ret2;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(const char *, struct hostent *, char *, size_t , struct hostent **, int *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("gethostbyname_r");
  ret2 = orig_func(name, ret, buf, buflen, result, h_errnop);
#else
  ret2 = gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
#endif
  error = errno;
  return ret2;
}

int Runtime::__getaddrinfo(unsigned insid, int &error, const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("getaddrinfo");
  ret = orig_func(node, service, hints, res);
#else
  ret = getaddrinfo(node, service, hints, res);
#endif
  error = errno;
  return ret;
}

void  Runtime::__freeaddrinfo(unsigned insid, int &error, struct addrinfo *res) {
  errno = error;
#ifdef XTERN_PLUS_DBUG
  typedef void (*orig_func_type)(struct addrinfo *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("freeaddrinfo");
  orig_func(res);
#else
  freeaddrinfo(res);
#endif
  error = errno;
}

struct hostent *Runtime::__gethostbyaddr(unsigned ins, int &error, const void *addr, int len, int type)
{
  errno = error;
  struct hostent *ret;
#ifdef XTERN_PLUS_DBUG
  typedef struct hostent * (*orig_func_type)(const void *, int, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("gethostbyaddr");
  ret = orig_func(addr, len, type);
#else
  ret = gethostbyaddr(addr, len, type);
#endif
  error = errno;
  return ret;
}

char *Runtime::__inet_ntoa(unsigned ins, int &error, struct in_addr in)
{
  errno = error;
  char *ret;
#ifdef XTERN_PLUS_DBUG
  typedef  char * (*orig_func_type)(struct in_addr);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("inet_ntoa");
  ret = orig_func(in);
#else
  ret = inet_ntoa(in);
#endif
  error = errno;
  return ret;
}

char *Runtime::__strtok(unsigned ins, int &error, char * str, const char * delimiters)
{
  errno = error;
  char *ret;
#ifdef XTERN_PLUS_DBUG
  typedef  char * (*orig_func_type)(char *, const char *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("strtok");
  ret = orig_func(str, delimiters);
#else
  ret = strtok(str, delimiters);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__send(unsigned ins, int &error, int sockfd, const void *buf, size_t len, int flags)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int, const void*, size_t, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("send");
  dprintf("Runtime::__send begin\n");
  ret = orig_func(sockfd, buf, len, flags);
  dprintf("Runtime::__send end\n");
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
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int, int );
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("shutdown");
  ret = orig_func(sockfd,how);
#else
  ret = ::shutdown(sockfd,how);
#endif
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

ssize_t Runtime::__pread(unsigned ins, int &error, int fd, void *buf, size_t count, off_t offset)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int, void*, size_t, off_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pread");
  ret = orig_func(fd, buf, count, offset);
#else
  ret = pread(fd, buf, count, offset);
#endif
  error = errno;
  return ret;
}

ssize_t Runtime::__pwrite(unsigned ins, int &error, int fd, const void *buf, size_t count, off_t offset)
{
  errno = error;
  ssize_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef ssize_t (*orig_func_type)(int, const void*, size_t, off_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pwrite");
  ret = orig_func(fd, buf, count, offset);
#else
  ret = pwrite(fd, buf, count, offset);
#endif
  error = errno;
  return ret;
}

int Runtime::__select(unsigned ins, int &error, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, fd_set*, fd_set *, fd_set*, struct timeval*);
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

int Runtime::__poll(unsigned ins, int &error, struct pollfd *fds, nfds_t nfds, int timeout)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(struct pollfd *, nfds_t, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("poll");
  ret = orig_func(fds, nfds, timeout);
#else
  ret = poll(fds, nfds, timeout);
#endif
  error = errno;
  return ret;
}

int Runtime::__bind(unsigned ins, int &error, int socket, const struct sockaddr *address, socklen_t address_len)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int, const struct sockaddr *, socklen_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("bind");
  ret = orig_func(socket, address, address_len);
#else
  ret = bind(socket, address, address_len);
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
  ret = ::epoll_wait(epfd,events,maxevents,timeout);
#endif
  error = errno;
  return ret;
}

int Runtime::__epoll_create(unsigned ins, int &error, int size)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("epoll_create");
  dprintf("Runtime::__epoll_create calls into dbug.\n");
  ret = orig_func(size);
#else
  ret = ::epoll_create(size);
#endif
  error = errno;
  return ret;
}

int Runtime::__epoll_ctl(unsigned ins, int &error, int epfd, int op, int fd, struct epoll_event *event)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(int , int , int , struct epoll_event *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("epoll_ctl");
  dprintf("Runtime::__epoll_ctl calls into dbug.\n");
  ret = orig_func(epfd,op,fd,event);
#else
  ret = ::epoll_ctl(epfd,op,fd,event);
#endif
  error = errno;
  return ret;
}

unsigned int Runtime::__sleep(unsigned ins, int &error, unsigned int seconds)
{
  errno = error;
  unsigned int ret;
#ifdef XTERN_PLUS_DBUG
  typedef unsigned int (*orig_func_type)(unsigned int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sleep");
  ret = orig_func(seconds);
#else
  ret = ::sleep(seconds);
#endif
  error = errno;
  return ret;
}

int Runtime::__usleep(unsigned ins, int &error, useconds_t usec)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(useconds_t);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("usleep");
  ret = orig_func(usec);
#else
  ret = ::usleep(usec);
#endif
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

void Runtime::___exit(int status) {
#ifdef XTERN_PLUS_DBUG
  typedef void (*orig_func_type)(int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("_exit");
  fprintf(stderr, "Calling Runtime::___exit into dbug\n");
  orig_func(status);
  fprintf(stderr, "Calling Runtime::___exit from dbug\n");
#else
  _exit(status);
#endif
}

pid_t Runtime::__kill(unsigned ins, int &error, pid_t pid, int sig)
{
  errno = error;
  pid_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(pid_t, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("kill");
  dprintf("Calling Runtime::___kill into dbug\n");
  ret = orig_func(pid, sig);
#else
  ret = ::kill(pid, sig);
#endif
  error = errno;
  return ret;
}

pid_t Runtime::__fork(unsigned ins, int &error)
{
  errno = error;
  pid_t ret;
#ifdef XTERN_PLUS_DBUG
  Runtime::__attach_self_to_dbug(__FUNCTION__);
  typedef pid_t (*orig_func_type)();
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("fork");
  ret = orig_func();
  Runtime::__detach_self_from_dbug(__FUNCTION__);
#else
  ret = fork();
#endif
  error = errno;
  return ret;
}

int Runtime::__execv(unsigned ins, int &error, const char *path, char *const argv[]) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  Runtime::__attach_self_to_dbug(__FUNCTION__);
  typedef pid_t (*orig_func_type)(const char *path, char *const argv[]);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("execv");
  ret = orig_func(path, argv);
  // No detach from dbug here, because this is execv.
#else
  ret = ::execv(path, argv);
#endif
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

pid_t Runtime::__waitpid(unsigned ins, int &error, pid_t pid, int *status, int options)
{
  errno = error;
  pid_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef pid_t (*orig_func_type)(pid_t, int *, int);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("waitpid");
  ret = orig_func(pid, status, options);
#else
  ret = waitpid(pid, status, options);
#endif
  error = errno;
  return ret;
}

int Runtime::__sched_yield(unsigned ins, int &error) {
  errno = error;
  pid_t ret;
#ifdef XTERN_PLUS_DBUG
  typedef pid_t (*orig_func_type)();
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sched_yield");
  ret = orig_func();
#else
  ret = sched_yield();
#endif
  error = errno;
  return ret;
}

int Runtime::__nanosleep(unsigned ins, int &error, const struct timespec *req, struct timespec *rem)
{
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(const struct timespec *, struct timespec *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("nanosleep");
  ret = orig_func(req, rem);
#else
  ret = ::nanosleep(req, rem);
#endif
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

int Runtime::__sem_init(unsigned insid, int &error, sem_t *sem, int pshared, unsigned int value) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(sem_t *sem, int pshared, unsigned int value);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sem_init");
  dprintf("Runtime::%s pid %d, self %u start %p\n", __FUNCTION__, getpid(), (unsigned)pthread_self(), (void *)sem);
  ret = orig_func(sem, pshared, value);
  dprintf("Runtime::%s pid %d, self %u end\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
  ret = sem_init(sem, pshared, value); // Heming: do the "real" action here, since the latest dbug does not do 
  // this. And this may cause problem for some programs (use pthread barriers 
  // to enforce strong isolation safely, so some sync vars may be used in the 
  // deterministic regions).
#else
  ret = sem_init(sem, pshared, value);
#endif
  error = errno;
  return ret;
}

int Runtime::__sem_wait(unsigned insid, int &error, sem_t *sem) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(sem_t *sem);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sem_wait");
  dprintf("Runtime::%s pid %d, self %u start %p\n", __FUNCTION__, getpid(), (unsigned)pthread_self(), (void *)sem);
  ret = orig_func(sem);
  dprintf("Runtime::%s pid %d, self %u end\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
#else
  ret = sem_wait(sem);
#endif
  error = errno;
  return ret;
}

int Runtime::__sem_post(unsigned insid, int &error, sem_t *sem) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(sem_t *sem);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("sem_post");
  dprintf("Runtime::%s pid %d, self %u start %p\n", __FUNCTION__, getpid(), (unsigned)pthread_self(), (void *)sem);
  ret = orig_func(sem);
  dprintf("Runtime::%s pid %d, self %u end\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
#else
  ret = sem_post(sem);
#endif
  error = errno;
  return ret;
}

int Runtime::__pthread_mutex_init(unsigned insid, int &error, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(pthread_mutex_t *, const  pthread_mutexattr_t *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pthread_mutex_init");
  dprintf("Runtime::%s pid %d, self %u start %p\n", __FUNCTION__, getpid(), (unsigned)pthread_self(), (void *)mutex);
  ret = orig_func(mutex, mutexattr);
  dprintf("Runtime::%s pid %d, self %u end\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
  ret = pthread_mutex_init(mutex, mutexattr); // Heming: init the "real" mutex here, since the latest dbug does not do 
  // this. And this may cause problem for some programs (use pthread barriers 
  // to enforce strong isolation safely, so some sync vars may be used in the 
  // deterministic regions).
#else
  ret = pthread_mutex_init(mutex, mutexattr);
#endif
  error = errno;
  return ret;
}

int Runtime::__pthread_mutex_destroy(unsigned insid, int &error, pthread_mutex_t *mutex) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(pthread_mutex_t *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pthread_mutex_destroy");
  dprintf("Runtime::%s pid %d, self %u start\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
  ret = orig_func(mutex);
  dprintf("Runtime::%s pid %d, self %u end\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
  ret = pthread_mutex_destroy(mutex); // Heming: do the "real" action here, since the latest dbug does not do 
  // this. And this may cause problem for some programs (use pthread barriers 
  // to enforce strong isolation safely, so some sync vars may be used in the 
  // deterministic regions).
#else
  ret = pthread_mutex_destroy(mutex);
#endif
  error = errno;
  return ret;
}

int Runtime::__pthread_mutex_lock(unsigned insid, int &error, pthread_mutex_t *mutex) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(pthread_mutex_t *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pthread_mutex_lock");
  dprintf("Runtime::%s pid %d, self %u start mutex %p, ins %p\n", __FUNCTION__, getpid(), (unsigned)pthread_self(), (void *)mutex, (void *)insid);
  ret = orig_func(mutex);
  dprintf("Runtime::%s pid %d, self %u end\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
#else
  ret = pthread_mutex_lock(mutex);
#endif
  error = errno;
  return ret;
}

int Runtime::__pthread_mutex_unlock(unsigned insid, int &error, pthread_mutex_t *mutex) {
  errno = error;
  int ret;
#ifdef XTERN_PLUS_DBUG
  typedef int (*orig_func_type)(pthread_mutex_t *);
  static orig_func_type orig_func;
  if (!orig_func)
    orig_func = (orig_func_type)resolveDbugFunc("pthread_mutex_unlock");
  dprintf("Runtime::%s pid %d, self %u start\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
  ret = orig_func(mutex);
  dprintf("Runtime::%s pid %d, self %u end\n", __FUNCTION__, getpid(), (unsigned)pthread_self());
#else
  ret = pthread_mutex_unlock(mutex);
#endif
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


