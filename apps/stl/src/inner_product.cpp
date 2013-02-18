// inner_product algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/numeric>

// function generator:
int RandomNumber () { return (std::rand()%10); }


std::vector<int> myvector(1000*1000*100);
std::vector<int> result(1000*1000*100);
//std::vector<int> myvector(4);
//std::vector<int> result(4);

int main()
{
    int init = 10;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    //generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
    __gnu_parallel::generate (result.begin(), result.end(), RandomNumber);

    //__gnu_parallel::partial_sum(myvector.begin(), myvector.end(), result.begin());
    
//    std::cout << "myvector contains:";
//    for (std::vector<int>::iterator it=myvector.begin(); it != myvector.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
//
//    std::cout << "result contains:";
//    for (std::vector<int>::iterator it=result.begin(); it != result.end(); ++it)
//      std::cout << ' ' << *it;
//    std::cout << '\n';
 
    __gnu_parallel::inner_product(myvector.begin(), myvector.end(), result.begin(), init);

    return 0;
}
