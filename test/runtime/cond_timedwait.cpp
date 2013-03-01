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
#include <time.h>

pthread_mutex_t mu;
pthread_cond_t cv;

void* thread_func(void* arg) {
  struct timespec now;
  struct timespec next;
  clock_gettime(CLOCK_REALTIME, &now);
  tern_set_base_time(&now);
  next.tv_sec = now.tv_sec + 1; // timedwait for 1.234 second.
  next.tv_nsec = now.tv_nsec + 23400000;
  
  pthread_mutex_lock(&mu);
  pthread_cond_timedwait(&cv, &mu, &next);
  pthread_mutex_unlock(&mu);
}

int main(int argc, char *argv[], char* env[]) {
  int ret;
  const int nthreads = 1;
  pthread_t th[nthreads];

  pthread_mutex_init(&mu, NULL);
  pthread_cond_init(&cv, NULL);

  for (int i = 0; i < nthreads; i++) {
    ret  = pthread_create(&th[i], NULL, thread_func, NULL);
    assert(!ret && "pthread_create() failed!");
  }

  // Sleep for 3 seconds and signal().
  sleep(3);
  pthread_mutex_lock(&mu);
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mu);
  printf("DONE.\n");
  
  for (int i = 0; i < nthreads; i++)
    pthread_join(th[i], NULL);

  return 0;
}

// CHECK: DONE.
