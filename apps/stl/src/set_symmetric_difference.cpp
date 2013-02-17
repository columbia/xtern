// set_symmetric_difference example
#include <iostream>     // std::cout
#include <algorithm>    // std::set_union, std::sort
#include <vector>       // std::vector
#include <iterator>
#include <ctime>        // std::time
#include <parallel/algorithm>

//using namespace std;

#define DATA_SIZE 1000*1000*100
//#define DATA_SIZE 10

int RandomNumber () { return (std::rand()%10); }

  std::vector<int> first(DATA_SIZE);
  std::vector<int> second(DATA_SIZE);
  std::vector<int> v(DATA_SIZE*2);                     

int main () {
  //  int first[] = {5,10,15,20,25};
  //  int second[] = {50,40,30,20,10};
  //  std::vector<int> first(1000*1000*10);
  //  std::vector<int> second(1000*1000*10);
//    std::vector<int>::iterator it;
      
    std::srand ( unsigned ( std::time(0) ) );
    generate (first.begin(), first.end(), RandomNumber);
//    generate (second.begin(), second.end(), RandomNumber);  
  
    sort (first.begin(),first.end(), __gnu_parallel::sequential_tag()); 
//    sort (second.begin(),second.end(), __gnu_parallel::sequential_tag());
  
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    
//    std::vector<int>::iterator it;  
    __gnu_parallel::set_symmetric_difference(first.begin(), first.end(), second.begin(), second.end(), v.begin());
  
//    v.resize(it-v.begin()); 
//  
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
