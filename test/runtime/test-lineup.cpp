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

void loop(const char *tag) {
  const int loopCount = 50;
  for (int i = 0; i < loopCount; i++) {
    soba_wait(0);
    pthread_mutex_lock(&mu);
    printf("%s\n", tag);
    pthread_mutex_unlock(&mu);
  }
}

void* thread_func(void*) {
  loop("SECOND");
}

int main(int argc, char *argv[], char* env[]) {
  int ret;
  pthread_t th;
  soba_init(0, 2, 20);
  ret  = pthread_create(&th, NULL, thread_func, NULL);
  assert(!ret && "pthread_create() failed!");
  loop("FIRST");
  pthread_join(th, NULL);
  return 0;
}

// CHECK: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND
// CHECK-NEXT: SECOND
// CHECK-NEXT: FIRST
// CHECK-NEXT: FIRST
// CHECK-NEXT: SECOND

