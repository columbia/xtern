#ifndef __TERN_COMMON_RUNTIME_INTERFACE_H
#define __TERN_COMMON_RUNTIME_INTERFACE_H

/* -*- Mode: C++ -*- */

/* tern public methods used by a program; they are implemented by the
 * recorder and the replayer runtimes */

#ifdef __cplusplus
extern "C" {
#endif

  /* tern inserts this method to a program as the first static
   * constructor.  This method initializes tern internal data and marks
   * arguments passed to main() as symbolic */
  void tern_prog_begin(int argc, char **argv);
  /* cleans up tern internal data. @tern_prog_begin should call
   * atexit(@tern_prog_end) to schedule the clean up. */
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
