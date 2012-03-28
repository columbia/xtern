/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_HOOKS_H
#define __TERN_COMMON_HOOKS_H

/// hooks tern or developers insert to a multithreaded program.

#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>


#ifdef __cplusplus
extern "C" {
#endif

  //  used by idle thread to skip empty rounds
  void tern_idle_sleep();

  /// hooks that users insert translate into these
  void tern_symbolic_real(unsigned insid, void *addr,
                          int nbytes, const char *symname);
  void tern_task_begin_real(unsigned insid, void *addr,
                            int nbytes, const char *name);
  void tern_task_end_real(unsigned insid);

  /// hooks tern automatically inserts.  start with the ones tern provides
  void tern_prog_begin(void);   /// initializes tern internal data
  void tern_prog_end(void);     /// cleans up tern internal data
  void tern_thread_begin(void); /// called at the beginning of a thread
  void tern_thread_end(unsigned insid); /// called at the end of a thread

  void tern___libc_start_main(void *func_ptr, int argc, char* argv[], void *init_func,
    void *fini_func, void *stack_end);   /// called in dynamic hook mode

  /// tern inserts these methods to a target program to make races
  /// deterministic; these methods are implemented only by the replayer
  void tern_fix_up();
  void tern_fix_down();

  /// now the hooks to synchronization methods and blocking system
  /// calls. add one extra argument, @insid or instruction ID to each hook
  /// to ease debugging.
# undef DEF
# undef DEFTERNAUTO
# undef DEFTERNUSER
# define DEF(func, kind, rettype, args...) \
    rettype tern_ ## func (unsigned insid, ##args);
# define DEFTERNAUTO(func)
# define DEFTERNUSER(func)
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERNAUTO
# undef DEFTERNUSER

#ifdef __cplusplus
}
#endif

#endif
