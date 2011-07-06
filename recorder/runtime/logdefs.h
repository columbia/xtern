/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGDEFS_H
#define __TERN_RECORDER_LOGDEFS_H

namespace tern
{
  enum {
    TRUNK_SIZE      = 16*1024*1024U,   // 16MB
    LOG_SIZE        = 1*TRUNK_SIZE,
    RECORD_SIZE     = 32U,
    MAX_INLINE_ARGS = 2,
    MAX_EXTRA_ARGS  = 3
  };

  enum {
    InsidTy      = 0U,
    LoadTy       = 1U,
    StoreTy      = 2U,
    CallTy       = 3U,
    ExtraArgsTy  = 4U,
    RetTy        = 5U
  };

  static inline int NumExtraArgsRecord(int narg)
  {
    return (((narg)-MAX_INLINE_ARGS+MAX_EXTRA_ARGS-1)/MAX_EXTRA_ARGS);
  }
}

#endif
