// Parallel mode STL algorithms
// Name: find
// Function: Returns an iterator to the first element in the 
// range [first,last) that compares equal to val. If no such 
// element is found, the function returns last.
// Test case: Worst case (not found)
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>
#include "microbench.h"

// function generator:
//int RandomNumber () { return (std::rand()%100); }

//class generator:
struct c_unique {
  int current;
  c_unique() {current=0;}
  int operator()() {return ++current;}
} UniqueNumber;

unsigned int data_size = 0;

//std::vector<int> myvector(1000*1000*1000);
std::vector<int> myvector(data_size);
//std::vector<int> myvector(1000);

#define ITEM -1

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);

    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
  
  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
    gettimeofday(&start, NULL); 
    __gnu_parallel::find (myvector.begin(), myvector.end(), ITEM);
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
        - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
  
  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
   
    return 0;
}
