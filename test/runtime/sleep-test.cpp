// RUN: %srcroot/test/runtime/run-scheduler-test.py %s -gxx "%gxx" -llvmgcc "%llvmgcc" -projbindir "%projbindir" -ternruntime "%ternruntime"  -ternbcruntime "%ternbcruntime"

#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

int done;
pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

void* thread_func(void*) {
  for(int i=0; i<10; ++i) {
    pthread_mutex_lock(&mu);
    pthread_mutex_unlock(&mu);
    pthread_mutex_lock(&mu);
    pthread_mutex_unlock(&mu);
    done = 1;
  }
}

int main(int argc, char *argv[], char *env[]) {
  int ret;
  pthread_t th;
  struct timespec   ts;
  struct timeval    tp;

  ret  = pthread_create(&th, NULL, thread_func, NULL);
  assert(!ret && "pthread_create() failed!");

  done = 0;
  while(!done)
    sleep(1);

  done = 0;
  while(!done)
    usleep(1);

  done = 0;
  struct timespec t = {1, 1};
  while(!done)
    nanosleep(&t, NULL);

  pthread_join(th, NULL);
  return 0;
  return 0;
}



