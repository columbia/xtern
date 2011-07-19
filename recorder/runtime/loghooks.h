/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RECORDER_LOGHOOKS_H
#define __TERN_RECORDER_LOGHOOKS_H

#include <stdint.h>

extern "C" {

  void tern_log_insid(unsigned insid);
  void tern_log_load(unsigned insid, void* addr, uint64_t data);
  void tern_log_store(unsigned insid, void* addr, uint64_t data);
  void tern_log_call(uint8_t flags, unsigned insid,
                     short narg, void* func, ...);
  void tern_log_ret(uint8_t flags, unsigned insid,
                    short narg, void* func, uint64_t ret);

  /// The following three functions are for connecting the llvm::Function
  /// objects we statically see within a bitcode module with the function
  /// addresses logged at runtime.
  ///
  /// Statically within LLVM, we only see LLVM::Function objects and know
  /// their names, but we don't know their addresses at runtime.  At
  /// runtime, the program knows only addresses and sees no llvm::Function
  /// or function names.
  ///
  /// We solve this problem by creating two mappings:
  /// (1) name    <-> integer ID, used at analysis time
  /// (2) address <-> integer ID, used at runtime for logging
  ///
  /// The instrumentor loginstr creates mapping (1) while instrumenting a
  /// bc module.  It also writes a tern_funcs_call_logged() fucntion that
  /// builds mapping (2) at runtime.
  ///
  /// This is a little bit gross ...
  ///
  void tern_funcs_call_logged(void); ///will be replaced by loginstr
  void tern_func_call_logged(void* func, unsigned funcid, const char* name);
  void tern_func_escape(void* func, unsigned funcid, const char* name);
}

#endif
