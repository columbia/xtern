// Parallel mode STL algorithms
// Name: adjacent_find
// Function: Searches the range [first,last) for the first 
// occurrence of two consecutive elements that match, and returns an 
// iterator to the first of these two elements, or last if no such pair is found.
// Test case: Normal case (found)
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/numeric>
#include "microbench.h"

// function generator:
//int RandomNumber () { return (std::rand()%100); }

//class generator:
struct c_unique {
  int current;
  c_unique() {current=0;}
  int operator()() {return ++current;}
} UniqueNumber;

unsigned int data_size;

//std::vector<int> myvector(1000*1000*1000);
std::vector<int> myvector(data_size);
//std::vector<int> myvector(1000);

int main(int argc, char * argv[])
{
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);

    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    __gnu_parallel::generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
    *(myvector.end()-1) = *(myvector.end()-2);

    gettimeofday(&start, NULL); 
    __gnu_parallel::adjacent_find(myvector.begin(), myvector.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
        - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
    
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it != myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
// 
//    if(it == myvector.end())
//        std::cout << "no match" << "\n";
    return 0;
}
