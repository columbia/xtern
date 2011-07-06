/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOG_H
#define __TERN_RECORDER_LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  void tern_log_insid(int insid);
  void tern_log_loadstore(int insid, void* addr, uint64_t data);
  void tern_log_call(int insid, short narg, void* func, ...);
  void tern_log_ret(int insid, short narg, void* func, uint64_t ret);
  void tern_log_init(void);
  void tern_log_exit(void);

#ifdef __cplusplus
}
#endif

#endif
