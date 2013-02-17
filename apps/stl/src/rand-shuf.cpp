#include <stdio.h>
#include <vector>
#include <algorithm>
#include <omp.h>
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
  fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
  std::vector<int> vi(1000*1000*100);
  std::random_shuffle(vi.begin(), vi.end());

  int i = 0;
  std::vector<int>::iterator it;
  std::cout << "The first 100 numbers are :" << std::endl;
        for(it = vi.begin(); i < 100; i++, it++)
            std:cout << ' ' << *it; 

  return 0;
}
