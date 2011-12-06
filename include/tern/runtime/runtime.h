/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_H
#define __TERN_COMMON_RUNTIME_H

#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>

namespace tern {

struct Runtime {
  virtual void progBegin() {}
  virtual void progEnd() {}
  virtual void symbolic(unsigned insid, void *addr,
                        int nbytes, const char *name) {}
  virtual void threadBegin() {}
  virtual void threadEnd(unsigned insid) {}

  // thread
  virtual int pthreadCreate(unsigned insid, pthread_t *th, pthread_attr_t *a,
                            void *(*func)(void*), void *arg) = 0;
  virtual int pthreadJoin(unsigned insid, pthread_t th, void **retval) = 0;
  // no pthreadExit since it's handled via threadEnd()

  // mutex
  virtual int pthreadMutexLock(unsigned insid, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTryLock(unsigned insid, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTimedLock(unsigned insid, pthread_mutex_t *mu,
                                    const struct timespec *abstime) = 0;
  virtual int pthreadMutexUnlock(unsigned insid, pthread_mutex_t *mutex) = 0;

  // cond var
  virtual int pthreadCondWait(unsigned insid, pthread_cond_t *cv,
                              pthread_mutex_t *mu) = 0;
  virtual int pthreadCondTimedWait(unsigned insid, pthread_cond_t *cv,
                                   pthread_mutex_t *mu,
                                   const struct timespec *abstime) = 0;
  virtual int pthreadCondSignal(unsigned insid, pthread_cond_t *cv) = 0;
  virtual int pthreadCondBroadcast(unsigned insid, pthread_cond_t *cv) = 0;

  // barrier
  virtual int pthreadBarrierInit(unsigned insid, pthread_barrier_t *barrier,
                                 unsigned count) = 0;
  virtual int pthreadBarrierWait(unsigned insid,
                                 pthread_barrier_t *barrier) = 0;
  virtual int pthreadBarrierDestroy(unsigned insid,
                                    pthread_barrier_t *barrier) = 0;

  // semaphore
  virtual int semWait(unsigned insid, sem_t *sem) = 0;
  virtual int semTryWait(unsigned insid, sem_t *sem) = 0;
  virtual int semTimedWait(unsigned insid, sem_t *sem,
                           const struct timespec *abstime) = 0;
  virtual int semPost(unsigned insid, sem_t *sem) = 0;
  
  // socket and files
  virtual int __socket(unsigned ins, int domain, int type, int protocol);
  virtual int __listen(unsigned ins, int sockfd, int backlog);
  virtual int __accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen);
  virtual int __connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);
  //virtual struct hostent *__gethostbyname(unsigned ins, const char *name);
  //virtual struct hostent *__gethostbyaddr(unsigned ins, const void *addr, int len, int type);
  virtual ssize_t __send(unsigned ins, int sockfd, const void *buf, size_t len, int flags);
  virtual ssize_t __sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
  virtual ssize_t __sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags);
  virtual ssize_t __recv(unsigned ins, int sockfd, void *buf, size_t len, int flags);
  virtual ssize_t __recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
  virtual ssize_t __recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags);
  virtual int __shutdown(unsigned ins, int sockfd, int how);
  virtual int __getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen);  
  virtual int __getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen);
  virtual int __setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen);
  virtual int __close(unsigned ins, int fd);
  virtual ssize_t __read(unsigned ins, int fd, void *buf, size_t count);
  virtual ssize_t __write(unsigned ins, int fd, const void *buf, size_t count);
  virtual int __select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);  

  virtual int __sigwait(unsigned ins, const sigset_t *set, int *sig); 

  /// Installs a runtime as @the.  A runtime implementation must
  /// re-implement this function to install itself
  static void install();
  static Runtime *the;
};

extern void InstallRuntime();

}

#endif
