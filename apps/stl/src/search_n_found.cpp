// search_n algorithm example
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>
#include "microbench.h"

// function generator:
int RandomNumber () { return (std::rand()%100); }

//class generator:
//struct c_unique {
//  int current;
//  c_unique() {current=0;}
//  int operator()() {return ++current;}
//} UniqueNumber;

unsigned int data_size = 0;

//std::vector<int> myvector(1000*1000*1000);
std::vector<int> myvector(data_size);
//std::vector<int> myvector(1000);

// for vector size 100M with seed 1
#define ITEM 32 // 34 or 47 will appear 4 times with seed 1
#define TIMES 4 // max n for search_n

// for vector size 1B with seed 1: (4 times)
// 32 47 31 70 0 7 73 30 43 73

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);

    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    std::srand(SEED);
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());
  
//    //for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end()-5; ++it)
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end()-4; ++it)
////        if(*it == *(it+1) && *it == *(it+2) && *it == *(it+3) && *it == *(it+4) && *it == *(it+5))
//        if(*it == *(it+1) && *it == *(it+2) && *it == *(it+3) && *it == *(it+4))
//            std::cout << ' ' << *it; 
  
    gettimeofday(&start, NULL);
    __gnu_parallel::search_n (myvector.begin(), myvector.end(), TIMES, ITEM);
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
           - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
  
    return 0;
}
