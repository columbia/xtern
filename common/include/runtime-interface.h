/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_RUNTIME_INTERFACE_H
#define __TERN_COMMON_RUNTIME_INTERFACE_H

/* runtime interface between tern and a multithreaded program; this
 * interface is implemented by the recorder and the replayer runtimes */

#ifdef __cplusplus
extern "C" {
#endif

  /* initializes tern internal data */
  void tern_prog_begin(void);
  /* cleans up tern internal data. */
  void tern_prog_end(void);

  /* methods that users can manually insert */
# include "user.h"

  /* hooks to synchronization methods and blocking system calls.  add one
   * extra argument @insid for debugging */

  /* hooks to synchronization methods */
# include "hooks.h"

  /* tern inserts these methods to a target program to make races
   * deterministic; these methods are implemented only by the replayer */
  void tern_fix_up();
  void tern_fix_down();

#ifdef __cplusplus
}
#endif

#endif