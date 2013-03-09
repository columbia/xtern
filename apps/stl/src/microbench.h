#include <sys/time.h>   // gettimeofday
#include <cstdlib>      // std::rand, std::srand
#include <string.h>     // strcmp, memcpy

#define SEED 1

#define TINY    0xffffff
#define SMALL   0xfffffff
#define MEDIUM  0x3fffffff
#define LARGE   0xffffffff

#define SET_INPUT_SIZE(x, y) \
    if(x == 1) \
        data_size = LARGE; \
    else if(x == 2) { \
        if(!strcmp(y, "small")) \
            data_size = SMALL; \
        else if(!strcmp(y, "medium")) \
            data_size = MEDIUM;   \
        else if(!strcmp(y, "large")) \
            data_size = LARGE;  \
        else if(!strcmp(y, "tiny")) \
            data_size = TINY;  \
    }  
