/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_COMMON_USER_H
#define __TERN_COMMON_USER_H

/* users can choose to manually insert these methods to their programs */

#ifdef __cplusplus
extern "C" {
#endif

  /* mark memory [@addr, @addr+@nbytes) as symbolic; @symname names this
   * symbolic memory for debugging */
  void tern_symbolic(void *addr, int nbytes, const char *symname);

  /* for server programs, users manually insert these two methods at the
   * beginning and the end of the processing of a user request.  @addr and
   * @nbytes mark the relevant request data as symbolic.
   *
   * for batch programs, tern inserts these two methods automatically at
   * thread begin and end.  @addr is set to NULL.  */
  void tern_task_begin(void *addr, int nbytes, const char *name);
  void tern_task_end(void);

#ifdef __cplusplus
}
#endif

#endif
