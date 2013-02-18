// mismatch algorithm example
#include <iostream>     // std::cout
#include <algorithm>    // std::generate
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <parallel/algorithm>

// function generator:
//int RandomNumber () { return (std::rand()%100); }

//class generator:
//struct c_unique {
//  int current;
//  c_unique() {current=0;}
//  int operator()() {return ++current;}
//} UniqueNumber;

std::vector<int> myvector(1000*1000*100);
std::vector<int> second(1000*1000*100);
//std::vector<int> myvector(10);
//std::vector<int> second(10);

//#define ITEM 9999999

int main () {
//    bool a = false;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    myvector.push_back(10);    
    second.push_back(11);
//    std::pair<std::vector<int>::iterator,int*> mypair;
//    std::srand ( unsigned ( std::time(0) ) );
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
  
//    generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
//    generate (second.begin(), second.end(), UniqueNumber, __gnu_parallel::sequential_tag());

  //  std::cout << "myvector contains:";
  //  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
  //    std::cout << ' ' << *it;
  //  std::cout << '\n';
  
    __gnu_parallel::mismatch(myvector.begin(), myvector.end(), second.begin());
    
//    std::cout << *mypair.first << ":" << *mypair.second << "\n";
  
//    if(a)
//        std::cout << "equal \n";
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it)
//      std::cout << ' ' << *it;
//
//    std::cout << "counts " << a << '\n';
   
    return 0;
}
