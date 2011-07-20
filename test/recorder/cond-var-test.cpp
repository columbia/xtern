// RUN: %llvmgcc %s -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -o %t2
// RUN: llvm-dis -f %t2-record.bc

// test the x86 .a libraries
// RUN: llc -o %t2.s %t2-record.bc
// RUN: %gxx -o %t2 %t2.s -L %projlibdir -lcommonruntime -lrecruntime -lpthread
// RUN: ./%t2
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-0 -r -v > /dev/null
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-1 -r -v > /dev/null

// test the LLVM .bc modules
// RUN: llvm-ld -o %t3 %t2-record.bc %projlibdir/commonruntime.bc %projlibdir/recruntime.bc
// RUN: llvm-dis -f %t3.bc
// RUN: llc -o %t3.s %t3.bc
// RUN: %gxx -o %t3 %t3.s -lpthread
// RUN: ./%t3 | grep FIRST | grep SECOND

// stress
// RUN: ./%t3 && ./%t3 && ./%t3  && ./%t3  && ./%t3  && ./%t3  && ./%t3
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-0 -r -v > /dev/null
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-1 -r -v > /dev/null

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
int flag = 0;

void* thread_func(void*) {
  pthread_mutex_lock(&mu);
  write(1, "SECOND", 6);
  while(flag == 0)
    pthread_cond_wait(&cv, &mu);
  flag = 0;
  pthread_cond_broadcast(&cv);
  pthread_mutex_unlock(&mu);
}

int main(int argc, char *argv[], char *env[]) {
  int ret;
  pthread_t th;
  struct timespec   ts;
  struct timeval    tp;

  ret  = pthread_create(&th, NULL, thread_func, NULL);
  assert(!ret && "pthread_create() failed!");

  pthread_mutex_lock(&mu);
  write(1, "FIRST", 5);
  flag = 1;
  pthread_cond_signal(&cv);
  while(flag == 1) {
    gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000 + 100;
    pthread_cond_timedwait(&cv, &mu, &ts);
  }
  pthread_mutex_unlock(&mu);
  pthread_join(th, NULL);
  return 0;
}
