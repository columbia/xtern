// lexicographical_compare algorithm
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/numeric>

// function generator:
//char RandomNumber () { return (std::rand()%0xff); }


std::vector<int> myvector(1000*1000*100);
std::vector<int> result(1000*1000*100);
//std::vector<char> myvector(4);
//std::vector<char> result(4);

int main()
{
//    bool a = false;
    char ch = 0xdd;
    fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
    myvector.push_back(ch);
    result.push_back(ch+1);
    //generate (myvector.begin(), myvector.end(), UniqueNumber, __gnu_parallel::sequential_tag());
//    __gnu_parallel::generate (myvector.begin(), myvector.end(), RandomNumber);
//    __gnu_parallel::generate (result.begin(), result.end(), RandomNumber);

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
 
    __gnu_parallel::lexicographical_compare(myvector.begin(), myvector.end(), result.begin(), result.end());
//    if(a)
//        std::cout << "equal" << '\n';

    return 0;
}
