// find algorithm example
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

#define ITEM 9999999

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);
    
    struct timeval start, end;
    int n = omp_get_max_threads();
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
  
  //  __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
  
  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
  
//    __gnu_parallel::generate(myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());

    *(myvector.begin() + (unsigned int) data_size/n * (n-1) - 1) = ITEM;

    gettimeofday(&start, NULL);
    __gnu_parallel::find (myvector.begin(), myvector.end(), ITEM);
    gettimeofday(&end, NULL); 
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
        - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);

//    if (it != myvector.end())
//        std::cout << "found \n";
//    std::cout << "p is " << *it;
  
  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
   
    return 0;
}
