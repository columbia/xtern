// RUN: %llvmgcc %s -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -o %t2
// RUN: llvm-dis -f %t2-record.bc

// test the x86 .a libraries
// RUN: llc -o %t2.s %t2-record.bc
// RUN: %gxx -o %t2 %t2.s -L %projlibdir -lcommonruntime -lrecruntime -lpthread
// RUN: ./%t2 | sort > %t2.out
// RUN: diff %t2.out %s.out
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-0 -r -v > /dev/null
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-1 -r -v > /dev/null
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-2 -r -v > /dev/null

// stress
// RUN: ./%t2 && ./%t2 && ./%t2  && ./%t2  && ./%t2  && ./%t2  && ./%t2

// test the LLVM .bc modules
// RUN: llvm-ld -o %t3 %t2-record.bc %projlibdir/commonruntime.bc %projlibdir/recruntime.bc
// RUN: llvm-dis -f %t3.bc
// RUN: llc -o %t3.s %t3.bc
// RUN: %gxx -o %t3 %t3.s -lpthread
// RUN: ./%t3 | sort > %t3.out
// RUN: diff %t3.out %s.out
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-0 -r -v > /dev/null
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-1 -r -v > /dev/null
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-2 -r -v > /dev/null

// stress
// RUN: ./%t3 && ./%t3 && ./%t3  && ./%t3  && ./%t3  && ./%t3  && ./%t3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#define N (1000)

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void* thread_func(void *arg) {
  char buf[64];
  int tid = (intptr_t)arg;

  for(unsigned i=0;i<100;++i)
    sched_yield();

  sprintf(buf, "%03d RUNS\n", tid);
  pthread_mutex_lock(&m);
  write(1, buf, strlen(buf));
  pthread_mutex_unlock(&m);
}

int main(int argc, char *argv[], char *env[]) {
  int ret;
  pthread_t th[N];

  for(unsigned i=0; i<N; ++i) {
    ret  = pthread_create(&th[i], NULL, thread_func, (void*)i);
    assert(!ret && "pthread_create() failed!");
  }

  for(unsigned i=0; i<N; ++i)
    pthread_join(th[i], NULL);
}
