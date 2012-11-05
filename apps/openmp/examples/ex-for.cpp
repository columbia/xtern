#include <stdio.h>

int main(int argc, char *argv[]) {
  const long N = 100000000;
  long long i, a[N];
  long long sum = 0;
 
  #pragma omp parallel for
  for (i = 0; i < N; i++) {
    a[i] = 2 * i * i * i * i * i * i * i * i * i * i; 
    //sum += a[i] * a[i];
  }
  fprintf(stderr, "sum %lld\n", sum);
  return 0;
}
