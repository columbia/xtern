#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "tern/user.h"

pthread_mutex_t mutex;
pthread_mutex_t mutex2;
pthread_barrier_t bar;

long sum = 0;

void delay() {
  //for (volatile int i = 0; i < 1e8; i++)
    //sum += i*i*i;
}

void *
thread(void *args)
{
  //assert(pthread_mutex_lock(&mutex) == 0);
  //printf("Critical section slave.\n");
  //assert(pthread_mutex_unlock(&mutex) == 0);
  pthread_barrier_wait(&bar);
  if ((long)args == 2) {
    fprintf(stderr, "start sleep for 5 seconds...\n");
    sleep(500);
    fprintf(stderr, "finish sleep for 5 seconds...\n");
    return NULL;
  }

  pcs_enter();
  int n = 0;
  fprintf(stderr, "\n\n\n\n\n\n\n\n\n============== start non-det self %u  =====================\n\n", (unsigned)pthread_self());
  delay();

  assert(pthread_mutex_lock(&mutex2) == 0);
  fprintf(stderr, "Critical section slave2.\n");
  assert(pthread_mutex_unlock(&mutex2) == 0);

  delay();

  fprintf(stderr, "\n\n============== end non-det self %u , sum %ld =====================\n\n\n\n\n\n\n\n\n\n", (unsigned)pthread_self(), sum);
  pcs_exit();

	/*for (int i = 0; i < 1e1; i++) {
      assert(pthread_mutex_lock(&mutex) == 0);
      //fprintf(stderr, "Critical section slave3 %d.\n", i);
      assert(pthread_mutex_unlock(&mutex) == 0);
  }*/

  //pthread_exit(0);
  return NULL;
}

int 
main(int argc, char *argv[])
{
  const int nt = 3;
  //return 0;
  int i;
  pthread_t tid[nt];
  assert(pthread_mutex_init(&mutex,NULL) == 0);
  pthread_barrier_init(&bar, NULL, nt+1);

  pcs_enter();
  assert(pthread_mutex_init(&mutex2,NULL) == 0);
  pcs_exit();
  for (i = 0; i < nt; i++)
    assert(pthread_create(&tid[i],NULL,thread,(void *)i) == 0);

  pthread_barrier_wait(&bar);


  //usleep(1);
	/*for (i = 0; i < 1e3; i++) {
      assert(pthread_mutex_lock(&mutex) == 0);
      //fprintf(stderr, "Critical section master %d.\n", i);
      assert(pthread_mutex_unlock(&mutex) == 0);
  }*/

  for (i = 0; i < nt; i++)
    assert(pthread_join(tid[i], NULL) == 0);

  assert(pthread_mutex_destroy(&mutex) == 0);

  return 0;
}

