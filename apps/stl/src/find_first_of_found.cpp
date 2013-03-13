// Parallel mode STL algorithms
// Name: find_first_of
// Function: Returns an iterator to the first element in 
// the range [first1,last1) that matches any of the elements 
// in[first2,last2). If no such element is found, the function returns last1.
// Test case: Normal case (found)
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>
#include "microbench.h"
#include <string.h>

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

#define SECOND_SIZE 2
#define ITEM 66666666

std::vector<int> second(SECOND_SIZE);

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);

    struct timeval start, end;
    int n = omp_get_max_threads();
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
//    unsigned int p =  data_size/n * (n-2) - 1;
//    std::cout << "p is " << p << "\n";
    *(myvector.begin() + (unsigned int)data_size/n * (n-2) - 1) = ITEM;
//    *(myvector.begin() + (int)data_size/n * (n-1) - 1) = ITEM + 1;
    
    *(second.begin()) = ITEM;
    *(second.end() - 1) = ITEM+1;
//    memcpy(&second[0], &myvector[START_OFF], SECOND_SIZE * sizeof(int));
  
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
  
    gettimeofday(&start, NULL);
    std::vector<int>::iterator it=    __gnu_parallel::find_first_of (myvector.begin(), myvector.end(), second.begin(), second.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);  

//    if(it != myvector.end())
//            std::cout << "found \n";

  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
   
    return 0;
}
