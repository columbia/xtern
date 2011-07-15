/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RECORDER_LOGHOOKS_H
#define __TERN_RECORDER_LOGHOOKS_H

#include <stdint.h>

extern "C" {

  void tern_log_insid(int insid);
  void tern_log_load(int insid, void* addr, uint64_t data);
  void tern_log_store(int insid, void* addr, uint64_t data);
  void tern_log_call(int indir, int insid, short narg, void* func, ...);
  void tern_log_ret(int indir, int insid, short narg, void* func, uint64_t ret);

  void tern_all_loggable_callees(void); ///will be replaced by loginstr
  void tern_loggable_callee(void* func, unsigned funcid);
  void tern_escape_callee(void* func, unsigned funcid);
}

#endif
