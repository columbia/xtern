/* -*- Mode: C++ -*- */

/* This is the central table that lists the synchronization functions and
 * blocking system calls that tern hooks, as well as tern builtin synch
 * functions. */

/* format:
 *   DEF(func, kind, ret_type, arg0, arg1, ...)
 *   DEFTERN(func)  for tern builtin sync events
 */


/* pthread synchronization operations */
DEF(pthread_create,         Synchronization, int, pthread_t *thread, pthread_attr_t *attr, void* (*start_routine)(void *), void *arg)
DEF(pthread_join,           Synchronization, int, pthread_t th, void **thread_return)
DEF(pthread_exit,           Synchronization, void, void *retval)
DEF(pthread_mutex_init,     Synchronization, int, pthread_mutex_t *mutex, const  pthread_mutexattr_t *mutexattr)
DEF(pthread_mutex_lock,     Synchronization, int, pthread_mutex_t *mutex)
DEF(pthread_mutex_unlock,   Synchronization, int, pthread_mutex_t *mutex)
DEF(pthread_mutex_trylock,  Synchronization, int, pthread_mutex_t *mutex)
DEF(pthread_mutex_timedlock,Synchronization, int, pthread_mutex_t *mutex, const struct timespec *abstime)
DEF(pthread_mutex_destroy,  Synchronization, int, pthread_mutex_t *mutex)
DEF(pthread_cond_init,      Synchronization, int, pthread_cond_t *cond, pthread_condattr_t*attr)
DEF(pthread_cond_wait,      Synchronization, int, pthread_cond_t *cond, pthread_mutex_t *mutex)
DEF(pthread_cond_timedwait, Synchronization, int, pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
DEF(pthread_cond_broadcast, Synchronization, int, pthread_cond_t *cond)
DEF(pthread_cond_signal,    Synchronization, int, pthread_cond_t *cond)
DEF(pthread_cond_destroy,   Synchronization, int, pthread_cond_t *cond)
DEF(pthread_barrier_init,   Synchronization, int, pthread_barrier_t *barrier, const pthread_barrierattr_t* attr, unsigned count)
DEF(pthread_barrier_wait,   Synchronization, int, pthread_barrier_t *barrier)
DEF(pthread_barrier_destroy,Synchronization, int, pthread_barrier_t *barrier)
DEF(sem_init,               Synchronization, int, sem_t *sem, int pshared, unsigned int value)
DEF(sem_post,               Synchronization, int, sem_t *sem)
DEF(sem_wait,               Synchronization, int, sem_t *sem)
DEF(sem_trywait,            Synchronization, int, sem_t *sem)
DEF(sem_timedwait,          Synchronization, int, sem_t *sem, const struct timespec *abs_timeout)
DEF(sem_destroy,            Synchronization, int, sem_t *sem)

/* blocking system calls */
DEF(sleep,                  BlockingSyscall, unsigned int, unsigned int seconds)
DEF(usleep,                 BlockingSyscall, int, useconds_t usec)
DEF(nanosleep,              BlockingSyscall, int, const struct timespec *req, struct timespec *rem)
DEF(accept,                 BlockingSyscall, int, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
DEF(select,                 BlockingSyscall, int, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
DEF(epoll_wait,             BlockingSyscall, int, int epfd, struct epoll_event *events, int maxevents, int timeout)
DEF(sigwait,                BlockingSyscall, int, const sigset_t *set, int *sig)
/* DEF(exit,                   BlockingSyscall, void, int status) */
/* DEF(syscall,                BlockingSyscall, tern_, int, int) */ /* FIXME: why include generic syscall entry point? */
/* DEF(ap_mpm_pod_check,       BlockingSyscall, tern_) */ /* FIXME: ap_mpm_pod_check is not a real lib call; needed for apache */

DEFTERN(tern_task_begin,   TernBuiltin)
DEFTERN(tern_task_end,     TernBuiltin)
DEFTERN(tern_fix_up,       TernBuiltin)
DEFTERN(tern_fix_down,     TernBuiltin)
