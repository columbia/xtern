// partial_sort algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/algorithm>

// function generator:
int RandomNumber () { return (std::rand()%100); }


std::vector<int> myvector(1000*1000*100);
//std::vector<int> myvector(100);

int main()
{
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    //generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
    __gnu_parallel::partial_sort(myvector.begin(), myvector.begin()+20, myvector.end());
    
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it < myvector.begin() + 30; ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
 
    return 0;
}
