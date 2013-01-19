// RUN: %srcroot/test/runtime/run-scheduler-test.py %s -gxx "%gxx" -llvmgcc "%llvmgcc" -projbindir "%projbindir" -ternruntime "%ternruntime" -ternannotlib "%ternannotlib"  -ternbcruntime "%ternbcruntime"

// Test the timeout of lineup.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include "tern/user.h"

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
const long workload = 1e8;

void loop(const char *tag) {
  int loopCount;
  if (!strcmp(tag, "T0"))
    loopCount = 20;
  else if (!strcmp(tag, "T1"))
    loopCount = 16;
  else if (!strcmp(tag, "T2"))
    loopCount = 13;
  else
    loopCount = 10;

  for (int i = 0; i < loopCount; i++) { 
    pthread_mutex_lock(&mu);
    pthread_mutex_unlock(&mu);

    tern_lineup_start(0);
    tern_lineup_end(0);   

    long sum = 0;
    for (long j = 0; j < workload; j++)
      sum += j*j;

    pthread_mutex_lock(&mu);
    printf("%s (%d) sum: %ld\n", tag, i, sum);
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

// Lineup 4 threads.
// CHECK: T3 (0) sum: 
// CHECK-NEXT: T0 (0) sum: 
// CHECK-NEXT: T1 (0) sum: 
// CHECK-NEXT: T2 (0) sum: 
// CHECK-NEXT: T2 (1) sum: 
// CHECK-NEXT: T3 (1) sum: 
// CHECK-NEXT: T0 (1) sum: 
// CHECK-NEXT: T1 (1) sum: 
// CHECK-NEXT: T1 (2) sum: 
// CHECK-NEXT: T2 (2) sum: 
// CHECK-NEXT: T3 (2) sum: 
// CHECK-NEXT: T0 (2) sum: 
// CHECK-NEXT: T0 (3) sum: 
// CHECK-NEXT: T1 (3) sum: 
// CHECK-NEXT: T2 (3) sum: 
// CHECK-NEXT: T3 (3) sum: 
// CHECK-NEXT: T3 (4) sum: 
// CHECK-NEXT: T0 (4) sum: 
// CHECK-NEXT: T1 (4) sum: 
// CHECK-NEXT: T2 (4) sum: 
// CHECK-NEXT: T2 (5) sum: 
// CHECK-NEXT: T3 (5) sum: 
// CHECK-NEXT: T0 (5) sum: 
// CHECK-NEXT: T1 (5) sum: 
// CHECK-NEXT: T1 (6) sum: 
// CHECK-NEXT: T2 (6) sum: 
// CHECK-NEXT: T3 (6) sum: 
// CHECK-NEXT: T0 (6) sum: 
// CHECK-NEXT: T0 (7) sum: 
// CHECK-NEXT: T1 (7) sum: 
// CHECK-NEXT: T2 (7) sum: 
// CHECK-NEXT: T3 (7) sum: 
// CHECK-NEXT: T3 (8) sum: 
// CHECK-NEXT: T0 (8) sum: 
// CHECK-NEXT: T1 (8) sum: 
// CHECK-NEXT: T2 (8) sum: 
// CHECK-NEXT: T2 (9) sum: 
// CHECK-NEXT: T3 (9) sum: 
// CHECK-NEXT: T0 (9) sum: 
// CHECK-NEXT: T1 (9) sum: 

// Lineup 3 threads.
// CHECK-NEXT: T2 (10) sum: 
// CHECK-NEXT: T0 (10) sum: 
// CHECK-NEXT: T1 (10) sum: 
// CHECK-NEXT: T2 (11) sum: 
// CHECK-NEXT: T0 (11) sum: 
// CHECK-NEXT: T1 (11) sum: 
// CHECK-NEXT: T2 (12) sum: 
// CHECK-NEXT: T0 (12) sum: 
// CHECK-NEXT: T1 (12) sum: 

// Lineup 2 threads.
// CHECK-NEXT: T0 (13) sum: 
// CHECK-NEXT: T1 (13) sum: 
// CHECK-NEXT: T0 (14) sum: 
// CHECK-NEXT: T1 (14) sum: 
// CHECK-NEXT: T0 (15) sum: 
// CHECK-NEXT: T1 (15) sum: 

// Only 1 thread.
// CHECK-NEXT: T0 (16) sum: 
// CHECK-NEXT: T0 (17) sum: 
// CHECK-NEXT: T0 (18) sum: 
// CHECK-NEXT: T0 (19) sum: 

