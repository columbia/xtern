/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "tern/config.h"
#include "tern/hooks.h"
#include "tern/runtime/runtime.h"
#include "tern/space.h"
#include "helper.h"
#include "tern/options.h"

using namespace tern;

extern "C" {

typedef void * (*thread_func_t)(void*);
static void *__tern_thread_func(void *arg) {

  void **args;
  void *ret_val;
  thread_func_t user_thread_func;
  void *user_thread_arg;

  args = (void**)arg;
  user_thread_func = (thread_func_t)((intptr_t)args[0]);
  user_thread_arg = args[1];
  // free arg before calling user_thread_func as it may not return (i.e.,
  // it may call pthread_exit())
  delete[] (void**)arg;

  tern_thread_begin();
  ret_val = user_thread_func(user_thread_arg);
  tern_pthread_exit(-1, ret_val); // calls tern_thread_end() and pthread_exit()
  assert(0 && "unreachable!");
}

int __tern_pthread_create(pthread_t *thread,  const pthread_attr_t *attr,
                          thread_func_t user_thread_func,
                          void *user_thread_arg) {
  void **args;

  // use heap because stack of this func may be deallocated before the
  // created thread reads the @args
  args = new void*[2];
  args[0] = (void*)(intptr_t)user_thread_func;
  args[1] = user_thread_arg;

  int ret;
  ret = pthread_create(thread, const_cast<pthread_attr_t*>(attr), __tern_thread_func, (void*)args);
  if(ret < 0)
    delete[] (void**)args; // clean up memory for @args
  return ret;
}

/*
    This idle thread is created to avoid empty runq.
    In the implementation with semaphore, there's a global token that must be 
    held by some threads. In the case that all threads are executing blocking
    function calls, the global token can be held by nothing. So we create this
    idle thread to ensure there's at least one thread in the runq to hold the 
    global token.

    Another solution is to add a flag in schedule showing if it happens that 
    runq is empty and the global token is held by no one. And recover the global
    token when some thread comes back to runq from blocking function call. 
 */
volatile int idle_done = 0;
pthread_t idle_th;
void *idle_thread(void *)
{
  while (!idle_done) {
    tern_usleep(0xdeadbeef, options::idle_sleep_length);
  }
  return NULL;
}

static pthread_t main_thread_th;
//  SYS -> SYS
//  must be called by the main thread
void __tern_prog_begin(void) {
  //fprintf(stderr, "%08d calls __tern_prog_begin\n", (int) pthread_self());
  assert(Space::isSys() && "__tern_prog_begin must start in sys space");

  main_thread_th = pthread_self();

  options::read_options("local.options");
  options::read_env_options();
  options::print_options("dump.options");

  tern::InstallRuntime();

  // FIXME: the version of uclibc in klee doesn't seem to pick up the
  // functions registered with atexit()
  //atexit(__tern_prog_end);

  tern_prog_begin();
  assert(Space::isSys());
  tern_thread_begin(); // main thread begins
  assert(Space::isApp());

  //  use tern_pthread_create because we want to fake the eip
  if (options::launch_idle_thread)
    tern_pthread_create(0xdead0000, &idle_th, NULL, idle_thread, NULL);
  assert(Space::isApp() && "__tern_prog_begin must end in app space");
}

//  SYS -> SYS
void __tern_prog_end (void) {
  //fprintf(stderr, "%08d calls __tern_prog_end\n", (int) pthread_self());
  assert(Space::isApp() && "__tern_prog_end must start in app space");

  // terminate the idle thread because it references the runtime which we
  // are about to free
  idle_done = 1;

  //  use tern_pthread_join because we want to fake the eip
  if (options::launch_idle_thread)
  {
    assert(pthread_self() != idle_th && "idle_th should never reach __tern_prog_end");
    tern_pthread_join(0xdeadffff, idle_th, NULL);
  }

  tern_thread_end(-1); // main thread ends
  assert(Space::isSys());
  tern_prog_end();

  delete tern::Runtime::the;
  tern::Runtime::the = NULL;
  assert(Space::isSys() && "__tern_prog_end must end in system space");
}

void __tern_symbolic(unsigned insid, void *addr,
                     int nbytes, const char *symname) {
  if(nbytes <= 0 || !addr)
    return;
  tern_symbolic_real(insid, addr, nbytes, symname);
}

void __tern_symbolic_argv(unsigned insid, int argc, char **argv) {
  char arg[32];
  for(int i=0; i<argc; ++i) {
    sprintf(arg, "arg%d\n", i);
    tern_symbolic_real(insid, argv[i], strlen(argv[i])+1, arg);
  }
}

}
