// partial_sort algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/algorithm>
#include "microbench.h"

// function generator:
int RandomNumber () { return (std::rand()%100); }

#define DATA_SIZE 1000*1000*100

unsigned int data_size = 0;

//std::vector<int> myvector(DATA_SIZE);
std::vector<int> myvector(data_size);
//std::vector<int> myvector(1000);

int main(int argc, char * argv[])
{
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);

    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    std::srand(SEED); 
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());

    gettimeofday(&start, NULL);
    __gnu_parallel::partial_sort(myvector.begin(), myvector.begin()+DATA_SIZE-1, myvector.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
        - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
    
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it < myvector.begin() + 30; ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
 
    return 0;
}
