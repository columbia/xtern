// RUN: %llvmgcc %s -g -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll > %t2.bc
// RUN: llvm-dis -f %t2.bc

// test the x86 .a libraries
// RUN: llc -o %t2.s %t2.bc
// RUN: g++ -g -o %t2 %t2.s -L %projlibdir -lcommonruntime -lrecruntime -lpthread
// RUN: ./%t2

// test the LLVM .bc modules
// RUN: llvm-ld -o %t3 %t2.bc %projlibdir/commonruntime.bc %projlibdir/recruntime.bc
// RUN: llvm-dis -f %t3.bc
// RUN: llc -o %t3.s %t3.bc
// RUN: g++ -g -o %t3 %t3.s -lpthread
// RUN: ./%t3 | sort > %t3.out
// RUN: diff %t3.out %s.out

// stress
// RUN: ./%t3 && ./%t3 && ./%t3  && ./%t3  && ./%t3  && ./%t3  && ./%t3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#define N (1000)

pthread_barrier_t ba;

void* thread_func(void* arg) {
  char buf[64];
  int tid = (intptr_t)arg;

  for(unsigned i=0;i<100;++i)
    sched_yield();

  sprintf(buf, "Arrived: %03d\n", tid);
  write(1, buf, strlen(buf));
  pthread_barrier_wait(&ba);
  sprintf(buf, "Left: %03d\n", tid);
  write(1, buf, strlen(buf));
}

int main() {
  int ret;
  pthread_t th[N];

  pthread_barrier_init(&ba, NULL, N);

  for(unsigned i=0; i<N; ++i) {
    ret  = pthread_create(&th[i], NULL, thread_func, (void*)i);
    assert(!ret && "pthread_create() failed!");
  }

  for(unsigned i=0; i<N; ++i)
    pthread_join(th[i], NULL);

  pthread_barrier_destroy(&ba);

  return 0;
}
