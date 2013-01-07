#include <stdio.h>
#include <vector>
#include <algorithm>
#include <omp.h>

int main(int argc, char *argv[]) {
  fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
  std::vector<int> vi(1000*1000*100);
  std::random_shuffle(vi.begin(), vi.end());
  return 0;
}
