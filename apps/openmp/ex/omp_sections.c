/*
  OpenMP example program demonstrating the use of the sections construct
  Compile with: gcc -fopenmp omp_sections.c -o omp_sections
*/

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#define N     50

int main (int argc, char *argv[]) {
  int i, nthreads, tid;
  float a[N], b[N], c[N], d[N];
  
  /* Some initializations */
  for (i=0; i<N; i++) {
    a[i] = i * 1.5;
    b[i] = i + 42.0;
    c[i] = d[i] = 0.0;
  }

  /* Start 2 threads */
#pragma omp parallel shared(a,b,c,d,nthreads) private(i,tid) num_threads(2)
  {
    tid = omp_get_thread_num();
    if (tid == 0) {
      nthreads = omp_get_num_threads();
      printf("Number of threads = %d\n", nthreads);
    }
    printf("Thread %d starting...\n",tid);
    
#pragma omp sections
    {
#pragma omp section
      {
	
	printf("Thread %d doing section 1\n",tid);
	for (i=0; i<N; i++) {
	  c[i] = a[i] + b[i];
	}
	sleep(tid+2);  /* Delay the thread for a few seconds */
	/* End of first section */
      }
#pragma omp section
      {
	printf("Thread %d doing section 2\n",tid);
	for (i=0; i<N; i++) {
	  d[i] = a[i] * b[i];
	}
	sleep(tid+2);   /* Delay the thread for a few seconds */
      } /* End of second section */
      
    }  /* end of sections */
    
    printf("Thread %d done.\n",tid); 

  }  /* end of omp parallel */

  /* Print the results */
  printf("c:  ");
  for (i=0; i<N; i++) {
    printf("%.2f ", c[i]);
  }
  
  printf("\n\nd:  ");
  for (i=0; i<N; i++) {
    printf("%.2f ", d[i]);
  }
  printf("\n");

  exit(0);
}
