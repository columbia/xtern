/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/epoll.h>

#include "hooks.h"

extern intptr_t unused;
void template(void)
{
# undef DEF
# undef DEFTERN
# define DEF(func, args...)  unused = (intptr_t) tern_ ## func;
# define DEFTERN(func, kind)
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERN
}
