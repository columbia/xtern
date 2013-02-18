// count_if algorithm example
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>

// function generator:
int RandomNumber () { return (std::rand()%100); }

//class generator:
//struct c_unique {
//  int current;
//  c_unique() {current=0;}
//  int operator()() {return ++current;}
//} UniqueNumber;

bool isOdd(int i) {
    return (i %2) == 1;
}

std::vector<int> myvector(1000*1000*100);
//std::vector<int> myvector(10);

//#define ITEM 9999999
#define ITEM 56

int main () {
//    int a = 0;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    myvector.push_back(1);
//    std::srand ( unsigned ( std::time(0) ) );
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
  
//    generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());

  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
  
    __gnu_parallel::count_if(myvector.begin(), myvector.end(), isOdd);
  
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//      std::cout << ' ' << *it;
//
//    std::cout << "counts " << a << '\n';
   
    return 0;
}
