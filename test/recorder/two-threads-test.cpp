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
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-0 -r -v > /dev/null
// RUN: %projbindir/logprint -bc %t2-analysis.bc tern-log-tid-1 -r -v > /dev/null

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

void* thread_func(void*) {
  pthread_mutex_lock(&mu);
  write(1, "SECOND", 6);
  pthread_mutex_unlock(&mu);
}

int main(int argc, char *argv[], char* env[]) {
  int ret;
  pthread_t th;

  ret  = pthread_create(&th, NULL, thread_func, NULL);
  assert(!ret && "pthread_create() failed!");

  pthread_mutex_lock(&mu);
  write(1, "FIRST", 5);
  pthread_mutex_unlock(&mu);

  pthread_join(th, NULL);
  return 0;
}
