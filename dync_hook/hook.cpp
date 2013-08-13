#include <pthread.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <execinfo.h>
#include <tern/hooks.h>
#include <tern/space.h>
#include <tern/options.h>
#include <tern/runtime/runtime.h>

using namespace tern;

#define __NEED_INPUT_INSID
#define __USE_TERN_RUNTIME
//#define PRINT_DEBUG

/*
#ifdef __USE_TERN_RUNTIME
#define __INSTALL_RUNTIME
#endif

#ifdef __INSTALL_RUNTIME

struct preload_static_block
{
  preload_static_block()
  {
    InstallRuntime();
  }
};

// notice, it doesn't work for functions called in static constructors of program 

static preload_static_block preload_initializer;

#endif
*/

#define __SPEC_HOOK___libc_start_main

static void print_stack()
{
#if 0
  void* tracePtrs[500];
  printf("here");
  fflush(stdout);
  int count = backtrace( tracePtrs, 500 );
  printf("here");
  fflush(stdout);
  char** funcNames = backtrace_symbols( tracePtrs, count );
  
  printf("here");
  fflush(stdout);
  // Print the stack trace
  for( int ii = 0; ii < count; ii++ )
    printf( "%s\n", funcNames[ii] );
  fflush(stdout);

  // Free the string pointers
  free( funcNames );
#endif
}

void *get_eip()
{
  // Change this "idx" to be 2 when pthread_*() functions are called by 
  // application code directly; and change it to be 3 when pthread_*()
  // functions are called by the app's wrapper functions.
  const int idx = 2;
  const int len = 5;
  void *tracePtrs[len];
  int i;
  uint64_t ret = 0; 
  /* Fixme: sometimes after fork(), the child process may hang (100% cpu) here, weird.
  Saw this case when running the simple-fork-test.c testcase. */
  int count = backtrace(tracePtrs, len);
      
  if (options::whole_stack_eip_signature)
  {
    ret = 0;
    for (i = 0; i < count; ++i)
      ret = ret * 97 + (uint64_t) tracePtrs[i];
    return (void*) ret;
    //std::cout << std::hex << tracePtrs[2] << std::dec << std::endl;
    //char** funcNames = backtrace_symbols( tracePtrs, count );
    //printf( "%s\n", funcNames[2] );
    //printf(stderr, "reteip: %p\n", tracePtrs[1]);
  } else
    return tracePtrs[idx];  //  this is ret_eip of my caller
}

#include "spec_hooks.cpp"
#include "template.h"
#include "template.cpp"
#include "annotation_hooks.cpp"

