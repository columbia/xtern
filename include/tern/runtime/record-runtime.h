/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
// Refactored from Heming's Memoizer code

#ifndef __TERN_RECORDER_RUNTIME_H
#define __TERN_RECORDER_RUNTIME_H

#include <tr1/unordered_map>
#include "tern/runtime/runtime.h"
#include "tern/runtime/record-scheduler.h"

namespace tern {

struct barrier_t {
  int count;    // barrier count
  int narrived; // number of threads arrived at the barrier
};
typedef std::tr1::unordered_map<pthread_barrier_t*, barrier_t> barrier_map;

template <typename _Scheduler>
struct RecorderRT: public Runtime, public _Scheduler {

  void progBegin(void);
  void progEnd(void);
  void threadBegin(void);
  void threadEnd(unsigned insid);

  // thread
  int pthreadCreate(unsigned insid, pthread_t *thread,  pthread_attr_t *attr,
                    void *(*thread_func)(void*), void *arg);
  int pthreadJoin(unsigned insid, pthread_t th, void **thread_return);

  // mutex
  int pthreadMutexLock(unsigned insid, pthread_mutex_t *mutex);
  int pthreadMutexTimedLock(unsigned insid, pthread_mutex_t *mutex,
                            const struct timespec *abstime);
  int pthreadMutexTryLock(unsigned insid, pthread_mutex_t *mutex);

  int pthreadMutexUnlock(unsigned insid, pthread_mutex_t *mutex);

  // cond var
  int pthreadCondWait(unsigned insid, pthread_cond_t *cv, pthread_mutex_t *mu);
  int pthreadCondTimedWait(unsigned insid, pthread_cond_t *cv,
                           pthread_mutex_t *mu, const struct timespec *abstime);
  int pthreadCondSignal(unsigned insid, pthread_cond_t *cv);
  int pthreadCondBroadcast(unsigned insid, pthread_cond_t *cv);

  // barrier
  int pthreadBarrierInit(unsigned insid, pthread_barrier_t *barrier, unsigned count);
  int pthreadBarrierWait(unsigned insid, pthread_barrier_t *barrier);
  int pthreadBarrierDestroy(unsigned insid, pthread_barrier_t *barrier);

  // semaphore
  int semWait(unsigned insid, sem_t *sem);
  int semTryWait(unsigned insid, sem_t *sem);
  int semTimedWait(unsigned insid, sem_t *sem, const struct timespec *abstime);
  int semPost(unsigned insid, sem_t *sem);

  void symbolic(unsigned insid, void *addr, int nbytes, const char *name);

  // socket & file
  int __socket(unsigned ins, int domain, int type, int protocol);
  int __listen(unsigned ins, int sockfd, int backlog);
  int __accept(unsigned ins, int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen);
  int __connect(unsigned ins, int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);
  //struct hostent *__gethostbyname(unsigned ins, const char *name);
  //struct hostent *__gethostbyaddr(unsigned ins, const void *addr, int len, int type);
  ssize_t __send(unsigned ins, int sockfd, const void *buf, size_t len, int flags);
  ssize_t __sendto(unsigned ins, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
  ssize_t __sendmsg(unsigned ins, int sockfd, const struct msghdr *msg, int flags);
  ssize_t __recv(unsigned ins, int sockfd, void *buf, size_t len, int flags);
  ssize_t __recvfrom(unsigned ins, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
  ssize_t __recvmsg(unsigned ins, int sockfd, struct msghdr *msg, int flags);
  int __shutdown(unsigned ins, int sockfd, int how);
  int __getpeername(unsigned ins, int sockfd, struct sockaddr *addr, socklen_t *addrlen);  
  int __getsockopt(unsigned ins, int sockfd, int level, int optname, void *optval, socklen_t *optlen);
  int __setsockopt(unsigned ins, int sockfd, int level, int optname, const void *optval, socklen_t optlen);
  int __close(unsigned ins, int fd);
  ssize_t __read(unsigned ins, int fd, void *buf, size_t count);
  ssize_t __write(unsigned ins, int fd, const void *buf, size_t count);
  int __select(unsigned ins, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);  
  
  RecorderRT(): _Scheduler(pthread_self()) {
    int ret = sem_init(&thread_create_sem, 0, 1); // main thread
    assert(!ret && "can't initialize semaphore!");
  }

protected:

  void pthreadMutexLockHelper(pthread_mutex_t *mutex);

  /// for each pthread barrier, track the count of the number and number
  /// of threads arrived at the barrier
  barrier_map barriers;

  //Logger logger;
  /// need this semaphore to assign tid deterministically; see
  /// pthreadCreate() and threadBegin()
  sem_t thread_create_sem;
};

} // namespace tern

#endif
