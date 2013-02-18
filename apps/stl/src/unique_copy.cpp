// unique_copy algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <ctime>        // std::time
#include <omp.h>
#include <iostream>
#include <parallel/numeric>

// function generator:
//int RandomNumber () { return (std::rand()%100); }


std::vector<int> myvector(1000*1000*100);
std::vector<int> result(1000*1000*100);
//std::vector<int> myvector(10);
//std::vector<int> result(10);

int main()
{
    std::vector<int>::iterator it;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    //generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());

//    std::srand ( unsigned ( std::time(0) ) );
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);

    //__gnu_parallel::partial_sum(myvector.begin(), myvector.end(), result.begin());
it=    __gnu_parallel::unique_copy(myvector.begin(), myvector.end(), result.begin());
    result.resize(std::distance(result.begin(),it));
    
//    std::cout << "myvector contains:";
//    for (it=myvector.begin(); it != myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//
//    std::cout << "result contains:";
//    for (std::vector<int>::iterator it=result.begin(); it != result.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
 
    return 0;
}
