/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOG_H
#define __TERN_RECORDER_LOG_H

#include <stdint.h>

#define TRUNK_SIZE   (16*1024*1024U)   // 16MB
#define LOG_SIZE     (1*TRUNK_SIZE)
#define RECORD_SIZE  (32U)             // record size

#define NUM_INLINE_ARGS (2)
#define NUM_EXTRA_ARGS  (3)

extern "C" {
  void tern_log(int insid, void* addr, uint64_t data);
  void tern_log_call(int insid, int narg, void* func, ...);
  void tern_log_ret(int insid, int narg, void* func, uint64_t data);
  void tern_log_init(void);
  void tern_log_exit(void);
}

#endif
