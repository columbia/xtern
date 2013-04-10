// RUN: %srcroot/test/runtime/run-scheduler-test.py %s -gxx "%gxx" -llvmgcc "%llvmgcc" -projbindir "%projbindir" -ternruntime "%ternruntime" -ternannotlib "%ternannotlib"  -ternbcruntime "%ternbcruntime"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <semaphore.h>
#include "tern/user.h"

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
long long sum;

void loop(const char *tag, long loopCount, long workLoad) {
  for (long i = 0; i < loopCount; i++) {
    soba_wait(0);

    for (long j = 0; j < workLoad; j++)
      sum += j*j;
      

    //pthread_mutex_lock(&mu);
    printf("%s\n", tag);
    //pthread_mutex_unlock(&mu);
  }
}

void* thread_func(void*) {
  loop("SECOND", 10, 1e8);
}

int main(int argc, char *argv[], char* env[]) {
  int ret;
  pthread_t th;
  soba_init(0, 2, 1000);
  ret  = pthread_create(&th, NULL, thread_func, NULL);
  assert(!ret && "pthread_create() failed!");
  loop("FIRST", 1, 1e9);
  pthread_join(th, NULL);
  return 0;
}
// CHECK: SECOND

