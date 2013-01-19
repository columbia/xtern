// RUN: %srcroot/test/runtime/run-scheduler-test.py %s -gxx "%gxx" -llvmgcc "%llvmgcc" -projbindir "%projbindir" -ternruntime "%ternruntime" -ternannotlib "%ternannotlib"  -ternbcruntime "%ternbcruntime"

// Lineup the computation blocks which are serialized by the xtern RR 
// scheduler. The overhead with xtern now is very little after adding this lineup.

#include <stdio.h>
#include "tern/user.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <semaphore.h>

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
const long workload = 1e8;

void loop(const char *tag) {
  const int loopCount = 10;
  for (int i = 0; i < loopCount; i++) { 
    pthread_mutex_lock(&mu);
    pthread_mutex_unlock(&mu);

    tern_lineup(0);   

    long sum = 0;
    for (long j = 0; j < workload; j++)
      sum += j*j;

    pthread_mutex_lock(&mu);
    printf("%s sum: %ld\n", tag, sum);
    pthread_mutex_unlock(&mu);
  }
}

void* thread_func(void* arg) {
  loop((const char *)arg);
}

int main(int argc, char *argv[], char* env[]) {
  int ret;
  const int nthreads = 4;
  pthread_t th[nthreads];
  const char *tags[nthreads] = {"T0", "T1", "T2", "T3"};

  tern_lineup_init(0, nthreads, 20);

  for (int i = 0; i < nthreads; i++) {
    ret  = pthread_create(&th[i], NULL, thread_func, (void *)tags[i]);
    assert(!ret && "pthread_create() failed!");
  }
  for (int i = 0; i < nthreads; i++)
    pthread_join(th[i], NULL);

  return 0;
}

// CHECK: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T2 sum: 
// CHECK-NEXT: T3 sum: 
// CHECK-NEXT: T0 sum: 
// CHECK-NEXT: T1 sum: 
