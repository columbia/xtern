// RUN: %llvmgcc %s -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -S -o %t2

// compare number of pthread calls and tern_pthread calls
// RUN: grep pthread_ %t1.ll  | grep call  | grep -v llvm.dbg.declare | wc -l > %t2.count1
// RUN: grep pthread %t2-record.ll  | grep tern_ | grep -v tern_log | grep -v tern_func | grep call | wc -l > %t2.count2
// RUN: diff %t2.count1 %t2.count2

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/epoll.h>
#include <semaphore.h>
#include <stdio.h>
#include <errno.h>


void* thread_func(void*) {
  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  sem_t s;
  pthread_t tid;
  pthread_cond_t c;
  pthread_mutex_t m;
  pthread_barrier_t b;
  timespec ts = {0, 10000};

  /* pthread synchronization operations */
  int ret = pthread_create(&tid, NULL, thread_func, NULL);
  if(ret < 0) {
    perror("pthread_create");
    abort();
  }

  pthread_join(tid, NULL);

  //pthread_mutex_init(&m, NULL);
  pthread_mutex_lock(&m);
  pthread_mutex_unlock(&m);
  pthread_mutex_trylock(&m);
  pthread_mutex_timedlock(&m, &ts);
  //pthread_mutex_destroy(&m);

  //pthread_cond_init(&c, NULL);
  pthread_cond_wait(&c, &m);
  pthread_cond_timedwait(&c, &m, &ts);
  pthread_cond_broadcast(&c);
  pthread_cond_signal(&c);
  //pthread_cond_destroy(&c);

  pthread_barrier_init(&b, NULL, 1);
  pthread_barrier_wait(&b);
  pthread_barrier_destroy(&b);

  //sem_init(&s, 0, 10);
  sem_post(&s);
  sem_wait(&s);
  sem_trywait(&s);
  sem_timedwait(&s, &ts);
  //sem_destroy(&s);

/* blocking system calls */
  sleep(1);
  usleep(1);
  nanosleep(&ts, &ts);
  accept(0, NULL, NULL);
  select(0, NULL, NULL, NULL, NULL);
  epoll_wait(0, NULL, 1, 0);
  sigwait(NULL, NULL);

  exit(0);
  return 0;
}
