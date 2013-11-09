// Parallel mode STL algorithms
// Name: random_shuffle
// Function: Rearranges the elements in the range [first,last) randomly.
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

//class generator:
//struct c_unique {
//  int current;
//  c_unique() {current=0;}
//  int operator()() {return ++current;}
//} UniqueNumber;

unsigned int data_size = 0;

//std::vector<int> myvector(1000*1000*100);
std::vector<int> myvector(data_size);
//std::vector<int> myvector(1000);

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);

    struct timeval start, end;
//    std::vector<int>::iterator it;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    std::srand ( unsigned ( std::time(0) ) );
  
  //  __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
  
  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
//    std::srand(SEED);
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());

    gettimeofday(&start, NULL);
    __gnu_parallel::random_shuffle (myvector.begin(), myvector.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);


//    __gnu_parallel::nth_element (myvector.begin(), myvector.begin()+5, myvector.end());
//    std::cout << "5th is " << myvector[5] << "\n";
//    std::cout << "vector length is " << myvector.size() << "\n";
//    std::cout << "last element is " << myvector.back() << "\n";
//  
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
   
    return 0;
}
