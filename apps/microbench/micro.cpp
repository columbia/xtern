#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#if defined(ENABLE_DMP)
  #include "dmp.h"
#else
  #include <pthread.h>
#endif

#include "tern/user.h"

#define MAX (100)

int T; // number of threads
int C; // computation size
int I = 1E7; // total number of iterations

pthread_t th[MAX];
pthread_mutex_t mu[MAX];

int compute(int C) {
  int x = 0;
  for(int i=0;i<C;++i)
    x = x * i;
  return x;
}

void* thread_func(void* arg) {
  long tid = (long)arg;
  for(int i=0; i<I; ++i) {
    pthread_mutex_lock(&mu[tid]);
    pthread_mutex_unlock(&mu[tid]);

    compute(C);
  }
}

extern "C" int main(int argc, char * argv[]);
int main(int argc, char *argv[]) {
  int ret;

  assert(argc == 3);
  T = atoi(argv[1]); assert(T <= MAX);
  C = atoi(argv[2]);

  for(long i=0; i<T; ++i) {
    pthread_mutex_init(&mu[i], NULL);
    ret  = pthread_create(&th[i], NULL, thread_func, (void*)i);
    assert(!ret && "pthread_create() failed!");
  }
  for(int i=0; i<T; ++i)
    pthread_join(th[i], NULL);
  return 0;
}
