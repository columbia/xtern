// Parallel mode STL algorithms
// Name: search
// Function: Searches the range [first1,last1) for the first 
// occurrence of the sequence defined by [first2,last2), and 
// returns an iterator to its first element, or last1 if no occurrences are found.
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
//std::vector<int> myvector(0xffffffff);
std::vector<int> myvector(data_size);
//std::vector<int> myvector(1000);

#define START_OFF 100000000       // old old
//#define START_OFF 1000000000    // old
//#define START_OFF 1000000
//#define SECOND_SIZE 1000000
#define SECOND_SIZE 500000

std::vector<int> second(SECOND_SIZE);

int main (int argc, char * argv[]) {
    SET_INPUT_SIZE(argc, argv[1])  
    myvector.resize(data_size); 

    struct timeval start, end;
//    int items[] = {ITEM, 11, 42, 29, 73, 21, 19, 84, 37, 98, 24, 15, 70, 13, 26, 91, 80, 56, 73, 62};
//    std::vector<int> second(items, items+19);
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    std::srand(SEED);
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
    for(int i = 0; i < SECOND_SIZE; i++)
        *(myvector.begin() + START_OFF + i) = 1;

//    int size = 1;

//    while(size < 0xffefffff) {

    memcpy(&second[0], &myvector[START_OFF], SECOND_SIZE * sizeof(int));
//    memcpy(&second[0], &myvector[START_OFF], size * sizeof(int));
//    size++;
//    fprintf(stderr, "size %d ");
  
//    std::cout << "second contains:";
//    for (std::vector<int>::iterator it=second.begin(); it!=second.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//    std::cout << *(second.end() - 1) << "\n";

//    std::vector<int>::iterator it;
  
    gettimeofday(&start, NULL);
    __gnu_parallel::search (myvector.begin(), myvector.end(), second.begin(), second.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
 
//    if( it != myvector.end())
//        std::cout << "Found" << *it << "\n";
//    std::cout << "myvector contains:";
//    std::vector<int>::iterator it;
//    for (it=myvector.begin(); it!=myvector.end() && *it != ITEM; ++it)
//      //std::cout << "\, " << *it;
//        ;
//    for(int i = 0; i < 20; i++)
//        std::cout << *(it+i) << ' ';
//    std::cout << '\n';
   
    return 0;
}
