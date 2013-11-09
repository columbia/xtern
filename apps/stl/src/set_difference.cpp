// Parallel mode STL algorithms
// Name: set_difference
// Function: Constructs a sorted range beginning in the location 
// pointed by result with the set difference of the sorted 
// range[first1,last1) with respect to the sorted range [first2,last2).
// Test case: Worst case (All elements)
#include <iostream>     // std::cout
#include <algorithm>    // std::set_union, std::sort
#include <vector>       // std::vector
#include <iterator>
#include <ctime>        // std::time
#include <parallel/algorithm>
#include "microbench.h"

#define DATA_SIZE 1000*1000*100
//#define DATA_SIZE 1000

//int RandomNumber () { return (std::rand()%100); }

//class generator:
struct c_unique {
  int current;
  c_unique() {current=1;}
  int operator()() {
    current += 2;  
    return current;
  }
} OddNumber;

struct c_unique_1 {
  int current;
  c_unique_1() {current=0;}
  int operator()() {
    current += 2;
    return current;
  }
} EvenNumber;

unsigned int data_size = 0;

//  std::vector<int> first(DATA_SIZE);
//  std::vector<int> second(DATA_SIZE);
//  std::vector<int> v(DATA_SIZE);                     
  std::vector<int> first(data_size);
  std::vector<int> second(data_size);
  std::vector<int> v(data_size);                     

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])
    first.resize(data_size);
    second.resize(data_size);
    v.resize(data_size);

    struct timeval start, end;  
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    __gnu_parallel::generate (first.begin(), first.end(), OddNumber, __gnu_parallel::sequential_tag());
//    __gnu_parallel::generate (second.begin(), second.end(), EvenNumber, __gnu_parallel::sequential_tag());
    
    gettimeofday(&start, NULL);
    __gnu_parallel::set_difference(first.begin(), first.end(), second.begin(), second.end(), v.begin());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);

  
//    for (it=first.begin(); it!=first.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//
//    for (it=second.begin(); it!=second.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//
//
//    std::cout << "The union has " << (v.size()) << " elements:\n";
//    for (it=v.begin(); it!=v.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
  
    return 0;
}
