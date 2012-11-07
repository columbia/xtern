/*
  OpenMP example program demonstrating threadprivate variables
  Compile with: gcc -O3 -fopenmp omp_threadprivate.c -o omp_threadprivate
*/

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int a, b, i, x, y, z, tid;
#pragma omp threadprivate(a,x, z)  /* a, x and z are threadprivate */


int main (int argc, char *argv[]) {

  /* Initialize the variables */
  a = b = x = y = z = 0;

  /* Fork a team of threads */
#pragma omp parallel private(b, tid) 
  {
    tid = omp_get_thread_num();
    a = b = tid;     /* a and b gets the value of the thread id */
    x = tid+10;      /* x is 10 plus the value of the thread id */
  }
  
  /* This section is now executed serially */
  for (i=0; i< 1000; i++) {
    y += i;
  }
  z = 40;  /* Initialize z outside the parallel region */
  

  /* Fork a new team of threads and initialize the threadprivate variable z */
#pragma omp parallel private(tid) copyin(z)
  {
    tid = omp_get_thread_num();
    z = z+tid;
    /* The variables a and x will keep their values from the last 
       parallel region but b will not. z will be initialized to 40 */
    printf("Thread %d:  a = %d  b = %d  x = %d z = %d\n", tid, a, b, x, z);
  }  /* All threads join master thread */

  exit(0);
}

