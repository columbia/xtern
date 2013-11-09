/* 
   OpenMP example program which computes the dot product of two arrays a and b
   (that is sum(a[i]*b[i]) ) using a sum reduction.
   Compile with gcc -O3 -fopenmp omp_reduction.c -o omp_reduction
*/

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 1000

int main (int argc, char *argv[]) {
  
  double a[N];
  double sum = 0.0;
  int i, n, tid;
  
  /* Start a number of threads */
#pragma omp parallel shared(a) private(i) 
  {
    tid = omp_get_thread_num();

    /* Only one of the threads do this */
#pragma omp single
    {
      n = omp_get_num_threads();
      printf("Number of threads = %d\n", n);
    }
    
    /* Initialize a */
#pragma omp for 
    for (i=0; i < N; i++) {
      a[i] = 1.0;
    }

    /* Parallel for loop computing the sum of a[i] */
#pragma omp for reduction(+:sum)
    for (i=0; i < N; i++) {
      sum = sum + (a[i]);
    }
    
  } /* End of parallel region */
  
  printf("   Sum = %2.1f\n",sum);
  exit(0);
}
