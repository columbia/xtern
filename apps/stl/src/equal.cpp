// equal algorithm example
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

unsigned int data_size = 0;

std::vector<int> myvector(data_size);
std::vector<int> second(data_size);
//std::vector<int> myvector(1000);
//std::vector<int> second(1000);


int main (int argc, char * argv[]) {
//    bool a = false;
    SET_INPUT_SIZE(argc, argv[1])
    myvector.resize(data_size);
    second.resize(data_size);

    struct timeval start, end;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
//    std::srand(SEED);
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());
//    std::srand(SEED);
//    __gnu_parallel::generate (second.begin(), second.end(), RandomNumber, __gnu_parallel::sequential_tag());
//    memcpy(&second[0], &myvector[0], myvector.size() * sizeof(int));

 
//    generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
//    generate (second.begin(), second.end(), UniqueNumber, __gnu_parallel::sequential_tag());

  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
  
    gettimeofday(&start, NULL);
    __gnu_parallel::equal(myvector.begin(), myvector.end(), second.begin());
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);


//    if(a)
//        std::cout << "equal \n";
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//      std::cout << ' ' << *it;
//
//    std::cout << "counts " << a << '\n';
   
    return 0;
}
