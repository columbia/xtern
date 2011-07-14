// RUN: %llvmgcc %s -g -O0 -c -o %t1.ll -S
// RUN: %projbindir/tern-instr < %t1.ll > %t2.bc
// RUN: llvm-dis -f %t2.bc
// RUN: llvm-ld -o %t3 %t2.bc %projlibdir/commonruntime.bc %projlibdir/recruntime.bc
// RUN: llvm-dis -f %t3.bc
// RUN: llc -o %t3.s %t3.bc
// RUN: g++ -g -o %t3 %t3.s -lpthread
// RUN: ./%t3 | grep FIRST | grep SECOND

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

int main() {
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
