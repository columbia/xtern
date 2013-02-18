#include <stdio.h>
#include <algorithm>
#include <vector>
#include <omp.h>
#include <iostream>
#include <parallel/algorithm>

using namespace std;

int main()
{
        fprintf(stderr, "omp num threads %d\n", omp_get_max_threads());
        std::vector<int> vi(1000*1000*100);
        __gnu_parallel::stable_sort(vi.begin(), vi.end());
//        std::vector<int>::iterator it;
//        int i = 0;

//        std::cout << "The first 100 numbers are :" << std::endl;
//        for(it = vi.begin(); i < 100; i++, it++)
//            std:cout << ' ' << *it; 
        return 0;
}
