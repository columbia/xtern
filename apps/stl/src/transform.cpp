// Parallel mode STL algorithms
// Name: transform
// Function: Applies an operation sequentially to the elements of 
// one (1) or two (2) ranges and stores the result in the range that begins at result.
// Test case: Worst case (All elements)
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>
#include "microbench.h"

// function generator:
int RandomNumber () { return (std::rand()%100); }

int myfunc(int i) {
    return ++i;
}

//class generator:
//struct c_unique {
//  int current;
//  c_unique() {current=0;}
//  int operator()() {return ++current;}
//} UniqueNumber;

unsigned int data_size = 0;

//std::vector<int> myvector(1000*1000*1000);
//std::vector<int> result(1000*1000*1000);
//std::vector<int> myvector(1000);
//std::vector<int> result(1000);
std::vector<int> myvector(data_size);
std::vector<int> result(data_size);

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);
    result.resize(data_size);

    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    std::srand(SEED);
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());
  
//    generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());

//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
  
    gettimeofday(&start, NULL);
    __gnu_parallel::transform(myvector.begin(), myvector.end(), result.begin(), myfunc);
    gettimeofday(&end, NULL); 
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
        - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
  
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=result.begin(); it!=result.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
   
    return 0;
}
