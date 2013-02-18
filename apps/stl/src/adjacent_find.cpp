// adjacent_find algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/numeric>

// function generator:
int RandomNumber () { return (std::rand()%10); }


std::vector<int> myvector(1000*1000*100);
//std::vector<int> myvector(10);

int main()
{
//    std::vector<int>::iterator it; 
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    //generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);

    //__gnu_parallel::partial_sum(myvector.begin(), myvector.end(), result.begin());
    __gnu_parallel::adjacent_find(myvector.begin(), myvector.end());
    
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it != myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
// 
//    if(it == myvector.end())
//        std::cout << "no match" << "\n";
    return 0;
}
