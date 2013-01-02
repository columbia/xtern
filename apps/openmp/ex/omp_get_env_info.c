/*
  OpenMP example program that reads and prints out some environment variables
  that control OpenMP execution.
  Compile with gcc -O3 -fopenmp omp_get_env_info.c -o omp_get_env_info
*/

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char *argv[]) 
{
  int nthreads, tid, procs, maxt, inpar, dynamic, nested;
  char name[50];
  
  /* Start parallel region */
#pragma omp parallel private(nthreads, tid)
  {
    
    /* Obtain thread number */
    tid = omp_get_thread_num();
    
    /* Only master thread does this
       We could also use #pragma omp master
     */
    if (tid == 0) 
      {
	printf("Thread %d getting environment info...\n", tid);
	
	/* Get host name */
	gethostname(name, 50);

	/* Get environment information */
	procs = omp_get_num_procs();
	nthreads = omp_get_num_threads();
	maxt = omp_get_max_threads();
	inpar = omp_in_parallel();
	dynamic = omp_get_dynamic();
	nested = omp_get_nested();
	
	/* Print environment information */
	printf("Hostname = %s\n", name);
	printf("Number of processors = %d\n", procs);
	printf("Number of threads = %d\n", nthreads);
	printf("Max threads = %d\n", maxt);
	printf("In parallel? = %d\n", inpar);
	printf("Dynamic threads enabled? = %d\n", dynamic);
	printf("Nested parallelism supported? = %d\n", nested);
	
      }
    
  }  /* Done */
  exit(0);
}

