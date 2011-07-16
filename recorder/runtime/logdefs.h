/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGDEFS_H
#define __TERN_RECORDER_LOGDEFS_H

#include <stdint.h>
#include <boost/static_assert.hpp>
#include "syncfuncs.h"

namespace tern {

#define RECORD_SIZE     (32U)  // has to be a define
#define INSID_BITS      (29U)
#define REC_TYPE_BITS   (3U)

enum {
  TRUNK_SIZE      = 1024*1024*1024U,   // 1GB
  LOG_SIZE        = 1*TRUNK_SIZE,
  MAX_INLINE_ARGS = 2U,
  MAX_EXTRA_ARGS  = 3U,
  MAX_INSID       = (1<<INSID_BITS),
  INVALID_INSID   = (unsigned)(-1)
};

enum {
  InsidRecTy      = 0U,
  LoadRecTy       = 1U,
  StoreRecTy      = 2U,
  CallRecTy       = 3U,
  ExtraArgsRecTy  = 4U,
  ReturnRecTy     = 5U,
  SyncRecTy       = 6U,

  LastRecTy       = SyncRecTy
};
BOOST_STATIC_ASSERT(LastRecTy<(1U<<REC_TYPE_BITS));

#ifdef ENABLE_PACKED_RECORD
#  pragma pack(push)  // push current alignment to stack
#  pragma pack(1)     // set alignment to 1 byte boundary
#endif

  struct InsidRec {
    unsigned insid  : INSID_BITS;
    unsigned type   : REC_TYPE_BITS;
  };
  BOOST_STATIC_ASSERT(sizeof(InsidRec)<=RECORD_SIZE);

  struct LoadRec: public InsidRec {
    void*    addr;
    uint64_t data;
  };
  BOOST_STATIC_ASSERT(sizeof(LoadRec)<=RECORD_SIZE);

  struct StoreRec: public InsidRec {
    void*    addr;
    uint64_t data;
  };
  BOOST_STATIC_ASSERT(sizeof(StoreRec)<=RECORD_SIZE);

  struct CallRec: public InsidRec {
    short    seq;
    short    narg;
    int      funcid;
    uint64_t args[MAX_INLINE_ARGS];
  };
  BOOST_STATIC_ASSERT(sizeof(CallRec)<=RECORD_SIZE);

  struct ExtraArgsRec: public InsidRec {
    short    seq;
    short    narg;
    uint64_t args[MAX_EXTRA_ARGS];
  };
  BOOST_STATIC_ASSERT(sizeof(ExtraArgsRec)<=RECORD_SIZE);

  struct ReturnRec: public InsidRec {
    short    seq;
    short    narg;
    int      funcid;
    uint64_t data;
  };
  BOOST_STATIC_ASSERT(sizeof(ReturnRec)<=RECORD_SIZE);

  struct SyncRec: public InsidRec {
    short    sync;   // type of sync call
    bool     after;  // before or after the sync call
    int      turn;   // turn no. that this sync occurred
    uint64_t args[MAX_INLINE_ARGS];
  };
  BOOST_STATIC_ASSERT(sizeof(SyncRec)<=RECORD_SIZE);

#ifdef ENABLE_PACKED_RECORD
#  pragma pack(pop)   // restore original alignment from stack
#endif

static inline int NumExtraArgsRecords(int narg) {
  return (((narg)-MAX_INLINE_ARGS+MAX_EXTRA_ARGS-1)/MAX_EXTRA_ARGS);
}

static inline int NumSyncArgs(short sync) {
  return (sync == syncfunc::pthread_cond_wait)? 2 : 1;
}

static inline int SetLogName(char *buf, size_t sz, int tid) {
  return snprintf(buf, sz, "tern-log-tid-%d", tid);
}

} // namespace tern

#endif
