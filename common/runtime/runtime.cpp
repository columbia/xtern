/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <assert.h>
#include "hooks.h"
#include "runtime.h"
#include "scheduler.h"

using namespace tern;

Runtime *Runtime::the = NULL;

int __thread TidMap::self_tid  = -1;

// make this a weak symbol so that a runtime implementation can replace it
void __attribute((weak)) InstallRuntime() {
  assert(0&&"A Runtime must define its own InstallRuntime() to instal itself!");
}

void tern_prog_begin() {
  tern::Runtime::the->progBegin();
}

void tern_prog_end() {
  tern::Runtime::the->progEnd();
}

void tern_symbolic_real(unsigned insid, void *addr,
                        int nbytes, const char *name) {
  tern::Runtime::the->symbolic(insid, addr, nbytes, name);
}

void tern_thread_begin(void) {
  tern::Runtime::the->threadBegin();
}

void tern_thread_end(unsigned ins) {
  tern::Runtime::the->threadEnd(ins);
}

int tern_pthread_create(unsigned ins, pthread_t *thread,  pthread_attr_t *attr,
                        void *(*thread_func)(void*), void *arg) {
  return tern::Runtime::the->pthreadCreate(ins, thread, attr,
                                           thread_func, arg);
}

int tern_pthread_join(unsigned ins, pthread_t th, void **retval) {
  return tern::Runtime::the->pthreadJoin(ins, th, retval);
}

int tern_pthread_mutex_lock(unsigned ins, pthread_mutex_t *mutex) {
  return tern::Runtime::the->pthreadMutexLock(ins, mutex);
}

int tern_pthread_mutex_trylock(unsigned ins, pthread_mutex_t *mutex) {
  return tern::Runtime::the->pthreadMutexTryLock(ins, mutex);
}

int tern_pthread_mutex_timedlock(unsigned ins, pthread_mutex_t *mutex,
                                 const struct timespec *t) {
  return tern::Runtime::the->pthreadMutexTimedLock(ins, mutex, t);
}

int tern_pthread_mutex_unlock(unsigned ins, pthread_mutex_t *mutex) {
  return tern::Runtime::the->pthreadMutexUnlock(ins, mutex);
}

int tern_pthread_cond_wait(unsigned ins, pthread_cond_t *cv,pthread_mutex_t *mu){
  return tern::Runtime::the->pthreadCondWait(ins, cv, mu);
}

int tern_pthread_cond_timedwait(unsigned ins, pthread_cond_t *cv,
                                pthread_mutex_t *mu, const struct timespec *t){
  return tern::Runtime::the->pthreadCondTimedWait(ins, cv, mu, t);
}

int tern_pthread_cond_signal(unsigned ins, pthread_cond_t *cv) {
  return tern::Runtime::the->pthreadCondSignal(ins, cv);
}

int tern_pthread_cond_broadcast(unsigned ins, pthread_cond_t *cv) {
  return tern::Runtime::the->pthreadCondBroadcast(ins, cv);
}

int tern_pthread_barrier_init(unsigned ins, pthread_barrier_t *barrier,
                        const pthread_barrierattr_t * attr, unsigned count) {
  return tern::Runtime::the->pthreadBarrierInit(ins, barrier, count);
}

int tern_pthread_barrier_wait(unsigned ins, pthread_barrier_t *barrier) {
  return tern::Runtime::the->pthreadBarrierWait(ins, barrier);
}

int tern_pthread_barrier_destroy(unsigned ins, pthread_barrier_t *barrier) {
  return tern::Runtime::the->pthreadBarrierDestroy(ins, barrier);
}

int tern_sem_wait(unsigned ins, sem_t *sem) {
  return tern::Runtime::the->semWait(ins, sem);
}

int tern_sem_trywait(unsigned ins, sem_t *sem) {
  return tern::Runtime::the->semTryWait(ins, sem);
}

int tern_sem_timedwait(unsigned ins, sem_t *sem, const struct timespec *t) {
  return tern::Runtime::the->semTimedWait(ins, sem, t);
}

int tern_sem_post(unsigned ins, sem_t *sem) {
  return tern::Runtime::the->semPost(ins, sem);
}

/// just a wrapper to tern_thread_end() and pthread_exit()
void tern_pthread_exit(unsigned ins, void *retval) {
  assert(tern::Scheduler::self() != tern::Scheduler::MainThreadTid
         && "calling pthread_exit() in main is currently not supported!");
  tern_thread_end(ins);
  pthread_exit(retval);
}
