// lexicographical_compare algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/numeric>
#include "microbench.h"
#include <string.h>

// function generator:
char RandomNumber () { return (std::rand()%100); }

unsigned int data_size = 0;

//std::vector<int> myvector(1000*1000*1000);
//std::vector<int> result(1000*1000*1000);
std::vector<int> myvector(data_size);
std::vector<int> result(data_size);
//std::vector<int> myvector(10);
//std::vector<int> result(10);

int main(int argc, char * argv[])
{
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);
    result.resize(data_size);

//    bool a = false;
    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    std::srand(SEED);
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());
//    std::srand(SEED);
//    __gnu_parallel::generate (result.begin(), result.end(), RandomNumber, __gnu_parallel::sequential_tag());
//    memcpy(&result[0], &myvector[0], myvector.size() * sizeof(int));


    //generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
//    __gnu_parallel::generate (result.begin(), result.end(), RandomNumber);

    
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it != myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//
//    std::cout << "result contains:";
//    for (std::vector<int>::iterator it=result.begin(); it != result.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
 
    gettimeofday(&start, NULL);
    __gnu_parallel::lexicographical_compare(myvector.begin(), myvector.end(), result.begin(), result.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
    
//    std::cout << a << '\n';
//    if(!a)
//        std::cout << "equal" << '\n';

    return 0;
}
