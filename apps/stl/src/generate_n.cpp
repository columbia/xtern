// generate algorithm example
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>

// function generator:
//int RandomNumber () { return (std::rand()%100); }

//class generator:
struct c_unique {
  int current;
  c_unique() {current=0;}
  int operator()() {return ++current;}
} UniqueNumber;

std::vector<int> myvector(1000*1000*100);
#define N_SIZE 1000*1000*99

int main () {
  fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
  std::srand ( unsigned ( std::time(0) ) );

//  __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);

//  std::cout << "myvector contains:";
//  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//    std::cout << ' ' << *it;
//  std::cout << '\n';

  __gnu_parallel::generate_n (myvector.begin(), N_SIZE, UniqueNumber);

//  std::cout << "myvector contains:";
//  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//    std::cout << ' ' << *it;
//  std::cout << '\n';
 
  return 0;
}
