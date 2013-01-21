/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_USER_H
#define __TERN_COMMON_USER_H

/* users manually insert these macros to their programs */

#ifdef __cplusplus
extern "C" {
#endif

  /// mark memory [@addr, @addr+@nbytes) as symbolic; @symname names this
  /// symbolic memory for debugging
  void tern_symbolic(void *addr, int nbytes, const char *symname);

  /// for server programs, users manually insert these two methods at
  /// beginning and the end of the processing of a user request.  @addr
  /// and @nbytes mark the relevant request data as symbolic.
  void tern_task_begin(void *addr, int nbytes, const char *name);
  void tern_task_end(void);

  /// New programming primitives to get better performance without affecting
  /// logics of applications.
  void tern_lineup_init(long opaque_type, unsigned count, unsigned timeout_turns);
  void tern_lineup_destroy(long opaque_type);
  void tern_lineup_start(long opaque_type);
  void tern_lineup_end(long opaque_type);
  void tern_lineup(long opaque_type);

  void tern_workload_start(long opaque_type, unsigned workload_hint);
  void tern_workload_end(long opaque_type);

  void tern_non_det_start();
  void tern_non_det_end();

#ifdef __cplusplus
}
#endif

#endif
