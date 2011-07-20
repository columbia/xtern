/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <assert.h>
#include "config.h"
#include "hooks.h"
#include "runtime.h"
#include "scheduler.h"
#include "helper.h"

#include <iostream>

using namespace tern;

Runtime *Runtime::the = NULL;

int __thread TidMap::self_tid  = -1;

// make this a weak symbol so that a runtime implementation can replace it
void __attribute((weak)) InstallRuntime() {
  assert(0&&"A Runtime must define its own InstallRuntime() to instal itself!");
}

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

int tern_pthread_create(unsigned ins, pthread_t *thread,  pthread_attr_t *attr,
                        void *(*thread_func)(void*), void *arg) {
  return Runtime::the->pthreadCreate(ins, thread, attr,
                                           thread_func, arg);
}

int tern_pthread_join(unsigned ins, pthread_t th, void **retval) {
  return Runtime::the->pthreadJoin(ins, th, retval);
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

/// just a wrapper to tern_thread_end() and pthread_exit()
void tern_pthread_exit(unsigned ins, void *retval) {
  // don't call tern_thread_end() for the main thread, since we'll call
  // tern_prog_end() later (which calls tern_thread_end())
  if(Scheduler::self() != Scheduler::MainThreadTid)
    tern_thread_end(ins);
  pthread_exit(retval);
}

void tern_exit(unsigned ins, int status) {
  tern_thread_end(ins); // main thread ends
  tern_prog_end();
  exit(status);
}
