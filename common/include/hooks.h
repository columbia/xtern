/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_HOOKS_H
#define __TERN_COMMON_HOOKS_H

/* declare hooks to synchronization methods and blocking system calls. add
 *  one extra argument @insid to each hook for debugging */

#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>


#ifdef __cplusplus
extern "C" {
#endif

# undef DEF
# undef DEFTERN
# define DEF(func, kind, rettype, args...) \
    rettype tern_ ## func (int insid, args);
# define DEFTERN(func, kind)
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERN


#ifdef __cplusplus
}
#endif

#endif
