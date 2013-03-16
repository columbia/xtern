/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_H
#define __TERN_COMMON_RUNTIME_H

#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace tern {

struct Runtime {
  Runtime();
  virtual void progBegin() {}
  virtual void progEnd() {}
  virtual void symbolic(unsigned insid, int &error, void *addr,
                        int nbytes, const char *name) {}
  virtual void threadBegin() {}
  virtual void threadEnd(unsigned insid) {}
  virtual void idle_sleep() {}
  virtual void idle_cond_wait() {}
  
  // thread
  virtual int pthreadCreate(unsigned insid, int &error, pthread_t *th, pthread_attr_t *a,
                            void *(*func)(void*), void *arg) = 0;
  virtual int pthreadJoin(unsigned insid, int &error, pthread_t th, void **retval) = 0;
  virtual int pthreadCancel(unsigned insid, int &error, pthread_t th);
  // no pthreadExit since it's handled via threadEnd()

  // mutex
  virtual int pthreadMutexInit(unsigned insid, int &error, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr);
  virtual int pthreadMutexDestroy(unsigned insid, int &error, pthread_mutex_t *mutex);
  virtual int pthreadMutexLock(unsigned insid, int &error, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTryLock(unsigned insid, int &error, pthread_mutex_t *mutex) = 0;
  virtual int pthreadMutexTimedLock(unsigned insid, int &error, pthread_mutex_t *mu,
                                    const struct timespec *abstime) = 0;
  virtual int pthreadMutexUnlock(unsigned insid, int &error, pthread_mutex_t *mutex) = 0;

  // cond var
  virtual int pthreadCondWait(unsigned insid, int &error, pthread_cond_t *cv,
                              pthread_mutex_t *mu) = 0;
  virtual int pthreadCondTimedWait(unsigned insid, int &error, pthread_cond_t *cv,
                                   pthread_mutex_t *mu,
                                   const struct timespec *abstime) = 0;
  virtual int pthreadCondSignal(unsigned insid, int &error, pthread_cond_t *cv) = 0;
  virtual int pthreadCondBroadcast(unsigned insid, int &error, pthread_cond_t *cv) = 0;

  // barrier
  virtual int pthreadBarrierInit(unsigned insid, int &error, pthread_barrier_t *barrier,
                                 unsigned count) = 0;
  virtual int pthreadBarrierWait(unsigned ins, int &error, 
                                 pthread_barrier_t *barrier) = 0;
  virtual int pthreadBarrierDestroy(unsigned ins, int &error, 
                                    pthread_barrier_t *barrier) = 0;

  // semaphore
  virtual int semWait(unsigned insid, int &error, sem_t *sem) = 0;
  virtual int semTryWait(unsigned insid, int &error, sem_t *sem) = 0;
  virtual int semTimedWait(unsigned insid, int &error, sem_t *sem,
                           const struct timespec *abstime) = 0;
  virtual int semPost(unsigned insid, int &error, sem_t *sem) = 0;

  // new programming primitives
  virtual void lineupInit(long opaque_type, unsigned count, unsigned timeout_turns) = 0;
  virtual void lineupDestroy(long opaque_type) = 0;
  virtual void lineupStart(long opaque_type) = 0;
  virtual void lineupEnd(long opaque_type) = 0;
  virtual void nonDetStart() = 0;
  virtual void nonDetEnd() = 0;
  virtual void setBaseTime(struct timespec *ts) = 0;

  // print runtime stat.
  virtual void printStat() = 0;

#ifdef XTERN_PLUS_DBUG
  static void *resolveDbugFunc(const char *func_name);
  static void initDbug();

  /* Special functions to attach/detach a thread from dbug. */
  static int __attach_self_to_dbug();
  static int __detach_self_from_dbug();
#endif


  virtual int __pthread_mutex_init(unsigned insid, int &error, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr);
  virtual int __pthread_mutex_destroy(unsigned insid, int &error, pthread_mutex_t *mutex);
  virtual int __pthread_mutex_lock(unsigned insid, int &error, pthread_mutex_t *mutex);
  virtual int __pthread_mutex_unlock(unsigned insid, int &error, pthread_mutex_t *mutex); 

/*
  virtual int __pthread_create(unsigned insid, int &error, pthread_t *th, const pthread_attr_t *a, void *(*func)(void*), void *arg)
  	{ return pthreadCreate(insid, th, const_cast<pthread_attr_t *>(a), func, arg); }
  virtual int __pthread_join(unsigned insid, int &error, pthread_t th, void **retval)
  	{ return pthreadJoin(insid, th, retval); }
  virtual int __pthread_cancel(unsigned insid, int &error, pthread_t th)
  	{ return pthreadCancel(insid, th); }
  virtual int __pthread_mutex_init(unsigned insid, int &error, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
  	{ return pthreadMutexInit(insid, mutex, mutexattr); }
  virtual int __pthread_mutex_destroy(unsigned insid, int &error, pthread_mutex_t *mutex)
  	{ return pthreadMutexDestroy(insid, mutex); }
  virtual int __pthread_mutex_lock(unsigned insid, int &error, pthread_mutex_t *mutex)
  	{ return pthreadMutexLock(insid, mutex); }
  virtual int __pthread_mutex_trylock(unsigned insid, int &error, pthread_mutex_t *mutex) 
  	{ return pthreadMutexTryLock(insid, mutex); }
  virtual int __pthread_mutex_timed_lock(unsigned insid, int &error, pthread_mutex_t *mu,
                                    const struct timespec *abstime) 
  	{ return pthreadMutexTimedLock(insid, mu, abstime); }
  virtual int __pthread_mutex_unlock(unsigned insid, int &error, pthread_mutex_t *mutex) 
  	{ return pthreadMutexUnlock(insid, mutex); }
  virtual int __pthread_cond_wait(unsigned insid, int &error, pthread_cond_t *cv,
                              pthread_mutex_t *mu) 
  	{ return pthreadCondWait(insid, cv, mu); }
  virtual int __pthread_cond_timedwait(unsigned insid, int &error, pthread_cond_t *cv,
                                   pthread_mutex_t *mu,
                                   const struct timespec *abstime) 
  	{ return pthreadCondTimedWait(insid, cv, mu, abstime); }
  virtual int __pthread_condSignal(unsigned insid, int &error, pthread_cond_t *cv) 
  	{ return pthreadCondSignal(insid, cv); }
  virtual int __pthread_cond_broadcast(unsigned insid, int &error, pthread_cond_t *cv)
  	{ return pthreadCondBroadcast(insid, cv); }

  // barrier
  virtual int __pthread_barrier_init(unsigned insid, int &error, pthread_barrier_t *barrier,
                                 unsigned count) 
  	{ return pthreadBarrierInit(insid, barrier, count); }
  virtual int __pthread_barrier_wait(unsigned ins, int &error, 
                                 pthread_barrier_t *barrier) 
  	{ return pthreadBarrierWait(insid, barrier); }
  virtual int __pthread_barrier_destroy(unsigned ins, int &error, 
                                    pthread_barrier_t *barrier)
  	{ return pthreadBarrierDestroy(insid, barrier); }

  // semaphore
  virtual int __sem_wait(unsigned insid, int &error, sem_t *sem) 
  	{ return semWait(insid, sem); }
  virtual int __sem_trywait(unsigned insid, int &error, sem_t *sem) 
  	{ return semTryWait(insid, sem); }
  virtual int __sem_timedwait(unsigned insid, int &error, sem_t *sem,
                           const struct timespec *abstime) 
  	{ return semTimedWait(insid, sem, abstime); }
  virtual int __sem_post(unsigned insid, int &error, sem_t *sem)
  	{ return semPost(insid, sem); }
*/

  // thread management
  static int __pthread_create(pthread_t *th, const pthread_attr_t *a, void *(*func)(void*), void *arg);
  static void __pthread_exit(void *value_ptr);
  static int __pthread_join(pthread_t th, void **retval);

  // socket and files
  virtual int __socket(unsigned insid, int &error, int domain, int type, int protocol);
  virtual int __listen(unsigned insid, int &error, int sockfd, int backlog);
  virtual int __accept(unsigned insid, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen);
  virtual int __accept4(unsigned insid, int &error, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen, int flags);
  virtual int __connect(unsigned insid, int &error, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);
  virtual struct hostent *__gethostbyname(unsigned insid, int &error, const char *name);
  virtual struct hostent *__gethostbyaddr(unsigned insid, int &error, const void *addr, int len, int type);
  virtual char *__inet_ntoa(unsigned ins, int &error, struct in_addr in);
  virtual char *__strtok(unsigned ins, int &error, char * str, const char * delimiters);
  virtual ssize_t __send(unsigned insid, int &error, int sockfd, const void *buf, size_t len, int flags);
  virtual ssize_t __sendto(unsigned insid, int &error, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
  virtual ssize_t __sendmsg(unsigned insid, int &error, int sockfd, const struct msghdr *msg, int flags);
  virtual ssize_t __recv(unsigned insid, int &error, int sockfd, void *buf, size_t len, int flags);
  virtual ssize_t __recvfrom(unsigned insid, int &error, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
  virtual ssize_t __recvmsg(unsigned insid, int &error, int sockfd, struct msghdr *msg, int flags);
  virtual int __shutdown(unsigned insid, int &error, int sockfd, int how);
  virtual int __getpeername(unsigned insid, int &error, int sockfd, struct sockaddr *addr, socklen_t *addrlen);  
  virtual int __getsockopt(unsigned insid, int &error, int sockfd, int level, int optname, void *optval, socklen_t *optlen);
  virtual int __setsockopt(unsigned insid, int &error, int sockfd, int level, int optname, const void *optval, socklen_t optlen);
  virtual int __close(unsigned insid, int &error, int fd);
  virtual ssize_t __read(unsigned insid, int &error, int fd, void *buf, size_t count);
  virtual ssize_t __write(unsigned insid, int &error, int fd, const void *buf, size_t count);
  virtual ssize_t __pread(unsigned insid, int &error, int fd, void *buf, size_t count, off_t offset);
  virtual ssize_t __pwrite(unsigned insid, int &error, int fd, const void *buf, size_t count, off_t offset);
  virtual int __select(unsigned insid, int &error, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);  
  virtual int __poll(unsigned ins, int &error, struct pollfd *fds, nfds_t nfds, int timeout);
  virtual int __bind(unsigned ins, int &error, int socket, const struct sockaddr *address, socklen_t address_len);
  virtual int __epoll_wait(unsigned insid, int &error, int epfd, struct epoll_event *events, int maxevents, int timeout);
  virtual int __sigwait(unsigned insid, int &error, const sigset_t *set, int *sig);
  virtual char *__fgets(unsigned ins, int &error, char *s, int size, FILE *stream);
  virtual pid_t __fork(unsigned ins, int &error);
  virtual pid_t __wait(unsigned ins, int &error, int *status);
  virtual pid_t __waitpid(unsigned ins, int &error, pid_t pid, int *status, int options);
  virtual time_t __time(unsigned ins, int &error, time_t *t);
  virtual int __clock_getres(unsigned ins, int &error, clockid_t clk_id, struct timespec *res);
  virtual int __clock_gettime(unsigned ins, int &error, clockid_t clk_id, struct timespec *tp);
  virtual int __clock_settime(unsigned ins, int &error, clockid_t clk_id, const struct timespec *tp);
  virtual int __gettimeofday(unsigned ins, int &error, struct timeval *tv, struct timezone *tz);
  virtual int __settimeofday(unsigned ins, int &error, const struct timeval *tv, const struct timezone *tz);
  // sleep
  virtual int schedYield(unsigned ins, int &error) = 0;
  virtual unsigned int sleep(unsigned insid, int &error, unsigned int seconds);
  virtual int usleep(unsigned insid, int &error, useconds_t usec);
  virtual int nanosleep(unsigned insid, int &error, const struct timespec *req, struct timespec *rem);
  virtual int __sched_yield(unsigned ins, int &error);

#define XDEF(op, sync, ret_type, args...)  \
  virtual ret_type __ ## op(unsigned insid, int &error, args);
XDEF(pthread_rwlock_rdlock, Synchronization, int, pthread_rwlock_t *rwlock)
XDEF(pthread_rwlock_tryrdlock, Synchronization, int, pthread_rwlock_t *rwlock)
XDEF(pthread_rwlock_trywrlock, Synchronization, int, pthread_rwlock_t *rwlock)
XDEF(pthread_rwlock_wrlock, Synchronization, int, pthread_rwlock_t *rwlock)
XDEF(pthread_rwlock_unlock, Synchronization, int, pthread_rwlock_t *rwlock)
XDEF(pthread_rwlock_destroy, Synchronization, int, pthread_rwlock_t *rwlock)
XDEF(pthread_rwlock_init, Synchronization, int, pthread_rwlock_t * rwlock, const pthread_rwlockattr_t * attr) 
#undef XDEF

  /// Installs a runtime as @the.  A runtime implementation must
  /// re-implement this function to install itself
  static void install();
  static Runtime *the;
};

extern void InstallRuntime();

}

#endif
