// inner_product algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/numeric>
#include "microbench.h"
#include <string.h>

// function generator:
int RandomNumber () { return (std::rand()%10); }


std::vector<int> myvector(1000*1000*600);
std::vector<int> second(1000*1000*600);
//std::vector<int> myvector(1000);
//std::vector<int> result(1000);

int main()
{
    struct timeval start, end;
    int init = 10;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    //generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
    std::srand(SEED);
    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber, __gnu_parallel::sequential_tag());
 
//    std::srand(SEED);
//    __gnu_parallel::generate (second.begin(), second.end(), RandomNumber, __gnu_parallel::sequential_tag());
    memcpy(&second[0], &myvector[0], myvector.size() * sizeof(int));


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
 
    gettimeofday(&start, NULL);
    __gnu_parallel::inner_product(myvector.begin(), myvector.end(), second.begin(), init);
    gettimeofday(&end, NULL);
    fprintf(stderr, "real %.3f\n", ((end.tv_sec * 1000000 + end.tv_usec)
          - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);


    return 0;
}
