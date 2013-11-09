// TODO: fix logging
// XFAIL: *
// RUN: %llvmgcc %s -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll -o %t2
// RUN: llc -o %t2.s %t2-record.bc
// RUN: %gxx -o %t2 %t2.s %ternruntime -lpthread -lrt
// RUN: env TERN_OPTIONS=log_type=bin:output_dir=%t2.outdir ./%t2
// RUN: %projbindir/logprint -bc %t2-analysis.bc %t2.outdir/tid-0.bin -r -v | FileCheck %s.out

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#define N (2)

void bar() {
  pthread_exit(NULL);
}

void* thread_func(void *arg) {
  bar();
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

  exit(0);
  return 0;
}
