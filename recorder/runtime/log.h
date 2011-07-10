/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOG_H
#define __TERN_RECORDER_LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  void tern_log_insid(int insid);
  void tern_log_load(int insid, void* addr, uint64_t data);
  void tern_log_store(int insid, void* addr, uint64_t data);
  void tern_log_call(int indir, int insid, short narg, void* func, ...);
  void tern_log_ret(int indir, int insid, short narg, void* func, uint64_t ret);

  void tern_log_init();
  void tern_log_exit(void);
  void tern_log_thread_init(int tid);
  void tern_log_thread_exit(void);
  const char* tern_log_name(void);

  // this function will be replace by loginstr
  void tern_all_loggable_callees(void) __attribute((weak));

#ifdef __cplusplus
}
#endif

#endif
