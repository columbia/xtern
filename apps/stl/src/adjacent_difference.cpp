// adjacent_difference algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/numeric>
#include "microbench.h"

// function generator:
int RandomNumber () { return (std::rand()%100); }

unsigned int data_size = 0;

//std::vector<int> myvector(1000*1000*1000);
//std::vector<int> result(1000*1000*1000);
std::vector<int> myvector(data_size);
std::vector<int> result(data_size);
//std::vector<int> myvector(1000);
//std::vector<int> result(1000);

int main(int argc, char * argv[])
{
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);
    result.resize(data_size);

    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    std::srand(SEED);
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());


    gettimeofday(&start, NULL);
    __gnu_parallel::adjacent_difference(myvector.begin(), myvector.end(), result.begin());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
    
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it != myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//
//    std::cout << "result contains:";
//    for (std::vector<int>::iterator it=result.begin(); it != result.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
 
    return 0;
}
