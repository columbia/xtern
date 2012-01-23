// RUN: %srcroot/test/runtime/run-scheduler-test.py %s -gxx "%gxx" -llvmgcc "%llvmgcc" -projbindir "%projbindir" -ternruntime "%ternruntime"  -ternbcruntime "%ternbcruntime"

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
int flag = 0;

void* thread_func(void*) {
  pthread_mutex_lock(&mu);
  while(flag == 0)
    pthread_cond_wait(&cv, &mu);
  printf("SECOND\n");
  flag = 0;
  pthread_cond_broadcast(&cv);
  pthread_mutex_unlock(&mu);
}

int main(int argc, char *argv[], char *env[]) {
  int ret;
  pthread_t th;
  struct timespec   ts;
  struct timeval    tp;

  ret  = pthread_create(&th, NULL, thread_func, NULL);
  assert(!ret && "pthread_create() failed!");

  pthread_mutex_lock(&mu);
  flag = 1;
  printf("FIRST\n");
  pthread_cond_signal(&cv);
  while(flag == 1) {
    gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000 + 100;
    pthread_cond_timedwait(&cv, &mu, &ts);
  }
  printf("THIRD\n");
  pthread_mutex_unlock(&mu);
  pthread_join(th, NULL);
  return 0;
}

// OUT indicates expected output checked by  FileCheck; auto-generated by appending -gen to the RUN command above.
// OUT:      FIRST
// OUT-NEXT: SECOND
// OUT-NEXT: THIRD
// RR indicates expected RR schedule checked by  FileCheck; auto-generated by appending -gen to the RUN command above.
// RR:      pthread_create 1 0 0
// RR-NEXT: pthread_mutex_lock 3 0
// RR-NEXT: pthread_cond_signal 4 0
// RR-NEXT: pthread_cond_timedwait_first 5 0
// RR-NEXT: pthread_mutex_lock 6 1
// RR-NEXT: pthread_cond_broadcast 7 1
// RR-NEXT: pthread_mutex_unlock 8 1
// RR-NEXT: pthread_cond_timedwait_second 9 0
// RR-NEXT: pthread_mutex_unlock 11 0
// RR-NEXT: pthread_join 12 0
