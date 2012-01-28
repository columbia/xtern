/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_H
#define __TERN_COMMON_RUNTIME_H

#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/epoll.h>

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
  virtual int pthreadCancel(unsigned insid, pthread_t th);
  // no pthreadExit since it's handled via threadEnd()

  // mutex
  virtual int pthreadMutexInit(unsigned insid, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr);
  virtual int pthreadMutexDestroy(unsigned insid, pthread_mutex_t *mutex);
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

/*
  virtual int __pthread_create(unsigned insid, pthread_t *th, const pthread_attr_t *a, void *(*func)(void*), void *arg)
  	{ return pthreadCreate(insid, th, const_cast<pthread_attr_t *>(a), func, arg); }
  virtual int __pthread_join(unsigned insid, pthread_t th, void **retval)
  	{ return pthreadJoin(insid, th, retval); }
  virtual int __pthread_cancel(unsigned insid, pthread_t th)
  	{ return pthreadCancel(insid, th); }
  virtual int __pthread_mutex_init(unsigned insid, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
  	{ return pthreadMutexInit(insid, mutex, mutexattr); }
  virtual int __pthread_mutex_destroy(unsigned insid, pthread_mutex_t *mutex)
  	{ return pthreadMutexDestroy(insid, mutex); }
  virtual int __pthread_mutex_lock(unsigned insid, pthread_mutex_t *mutex)
  	{ return pthreadMutexLock(insid, mutex); }
  virtual int __pthread_mutex_trylock(unsigned insid, pthread_mutex_t *mutex) 
  	{ return pthreadMutexTryLock(insid, mutex); }
  virtual int __pthread_mutex_timed_lock(unsigned insid, pthread_mutex_t *mu,
                                    const struct timespec *abstime) 
  	{ return pthreadMutexTimedLock(insid, mu, abstime); }
  virtual int __pthread_mutex_unlock(unsigned insid, pthread_mutex_t *mutex) 
  	{ return pthreadMutexUnlock(insid, mutex); }
  virtual int __pthread_cond_wait(unsigned insid, pthread_cond_t *cv,
                              pthread_mutex_t *mu) 
  	{ return pthreadCondWait(insid, cv, mu); }
  virtual int __pthread_cond_timedwait(unsigned insid, pthread_cond_t *cv,
                                   pthread_mutex_t *mu,
                                   const struct timespec *abstime) 
  	{ return pthreadCondTimedWait(insid, cv, mu, abstime); }
  virtual int __pthread_condSignal(unsigned insid, pthread_cond_t *cv) 
  	{ return pthreadCondSignal(insid, cv); }
  virtual int __pthread_cond_broadcast(unsigned insid, pthread_cond_t *cv)
  	{ return pthreadCondBroadcast(insid, cv); }

  // barrier
  virtual int __pthread_barrier_init(unsigned insid, pthread_barrier_t *barrier,
                                 unsigned count) 
  	{ return pthreadBarrierInit(insid, barrier, count); }
  virtual int __pthread_barrier_wait(unsigned insid,
                                 pthread_barrier_t *barrier) 
  	{ return pthreadBarrierWait(insid, barrier); }
  virtual int __pthread_barrier_destroy(unsigned insid,
                                    pthread_barrier_t *barrier)
  	{ return pthreadBarrierDestroy(insid, barrier); }

  // semaphore
  virtual int __sem_wait(unsigned insid, sem_t *sem) 
  	{ return semWait(insid, sem); }
  virtual int __sem_trywait(unsigned insid, sem_t *sem) 
  	{ return semTryWait(insid, sem); }
  virtual int __sem_timedwait(unsigned insid, sem_t *sem,
                           const struct timespec *abstime) 
  	{ return semTimedWait(insid, sem, abstime); }
  virtual int __sem_post(unsigned insid, sem_t *sem)
  	{ return semPost(insid, sem); }
*/

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
    
  virtual int __epoll_wait(unsigned ins, int epfd, struct epoll_event *events, int maxevents, int timeout);

  virtual int __sigwait(unsigned ins, const sigset_t *set, int *sig); 

  // sleep
  virtual unsigned int sleep(unsigned ins, unsigned int seconds);
  virtual int usleep(unsigned ins, useconds_t usec);
  virtual int nanosleep(unsigned ins, const struct timespec *req, struct timespec *rem);

  /// Installs a runtime as @the.  A runtime implementation must
  /// re-implement this function to install itself
  static void install();
  static Runtime *the;
};

extern void InstallRuntime();

}

#endif
