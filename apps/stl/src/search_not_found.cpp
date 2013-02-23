// search algorithm example
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>
#include "microbench.h"
#include <string.h>

// function generator:
int RandomNumber () { return (std::rand()%100); }

//class generator:
//struct c_unique {
//  int current;
//  c_unique() {current=0;}
//  int operator()() {return ++current;}
//} UniqueNumber;

std::vector<int> myvector(1000*1000*1000);
//std::vector<int> myvector(1000);

#define START_OFF 10000
#define SECOND_SIZE 1000000
#define NOT_IN_ITEM 101

std::vector<int> second(SECOND_SIZE);

int main () {
    struct timeval start, end;
//    int items[] = {ITEM, 11, 42, 29, 73, 21, 19, 84, 37, 98, 24, 15, 70, 13, 26, 91, 80, 56, 73, 62};
//    std::vector<int> second(items, items+19);
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    std::srand(SEED);
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());

//    memcpy(&second[0], &myvector[START_OFF], SECOND_SIZE * sizeof(int));
    for (std::vector<int>::iterator it=second.begin(); it!=second.end(); ++it)
      *it = NOT_IN_ITEM;
  
//    std::cout << "second contains:";
//    for (std::vector<int>::iterator it=second.begin(); it!=second.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//    std::cout << *(second.end() - 1) << "\n";

  
    gettimeofday(&start, NULL);
    __gnu_parallel::search (myvector.begin(), myvector.end(), second.begin(), second.end());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);
 
//    std::cout << "Found" << *it << "\n";
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
