// replace algorithm example
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
//struct c_unique {
//  int current;
//  c_unique() {current=0;}
//  int operator()() {return ++current;}
//} UniqueNumber;

std::vector<int> myvector(1000*1000*100);
//std::vector<int> myvector(10);

#define ITEM 1

int main () {
    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    std::srand ( unsigned ( std::time(0) ) );
  
  //  __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
  
  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
  
    gettimeofday(&start, NULL);
    __gnu_parallel::replace(myvector.begin(), myvector.end(), 0, ITEM);
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);


//    std::cout << "Found" << *it << "\n";
  
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
   
    return 0;
}
