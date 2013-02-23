// set_intersection example
#include <iostream>     // std::cout
#include <algorithm>    // std::set_union, std::sort
#include <vector>       // std::vector
#include <iterator>
#include <ctime>        // std::time
#include <parallel/algorithm>
#include "microbench.h"


#define DATA_SIZE 1000*1000*500
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

// @INPUT:
// first: sequential odd number 
// sencond: sequential even number 
// Note: both inputs are sequential to reduce initialization time

  std::vector<int> first(DATA_SIZE);
  std::vector<int> second(DATA_SIZE);
  std::vector<int> v(DATA_SIZE);                     

int main () {
    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    
    __gnu_parallel::generate (first.begin(), first.end(), OddNumber, __gnu_parallel::sequential_tag());
    __gnu_parallel::generate (second.begin(), second.end(), EvenNumber, __gnu_parallel::sequential_tag());
 
    fprintf(stderr, "initialization done...\n");
//    std::vector<int>::iterator it;  
//    it = __gnu_parallel::set_intersection(first.begin(), first.end(), second.begin(), second.end(), v.begin());
    gettimeofday(&start, NULL);
    __gnu_parallel::set_intersection(first.begin(), first.end(), second.begin(), second.end(), v.begin());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);

 
//    v.resize(it-v.begin()); 
//  
//    std::cout << "The union has " << (v.size()) << " elements:\n";
//    for (it=v.begin(); it!=v.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
  
    return 0;
}
