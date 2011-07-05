/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOG_H
#define __TERN_RECORDER_LOG_H

#include <stdint.h>

#define TRUNK_SIZE (16*1024*1024U)   // 16MB
#define LOG_SIZE   (1*TRUNK_SIZE)
#define LOG_ALIGN  (32U)

#define ARG_READ   (1)
#define ARG_WRITE  (2)

extern "C" {
  void tern_log(int insid, void* addr, uint64_t data);
  void tern_log_init(void);
  void tern_log_exit(void);
}

#endif
