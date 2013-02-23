#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/algorithm>
#include "microbench.h"

// function generator:
int RandomNumber () { return (std::rand()%100); }

// @INPUT:
// myvector: random input
std::vector<int> myvector(1000*1000*100);

int main()
{
    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    std::srand(SEED);
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());


    gettimeofday(&start, NULL);
    __gnu_parallel::sort(myvector.begin(), myvector.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);


//        std::vector<int>::iterator it;
//        int i = 0;

//        std::cout << "The first 100 numbers are :" << std::endl;
//        for(it = vi.begin(); i < 100; i++, it++)
//            std:cout << ' ' << *it; 
    return 0;
}
