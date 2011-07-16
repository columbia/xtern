/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef __TERN_RECORDER_LOG_H
#define __TERN_RECORDER_LOG_H

#include <assert.h>
#include <stdarg.h>
#include <tr1/unordered_map>
#include "logdefs.h"

namespace tern {

struct Logger {
  /// per-thread logging functions and data
  void logInsid(unsigned insid);
  void logLoad(unsigned insid, void* addr, uint64_t data);
  void logStore(unsigned insid, void* addr, uint64_t data);
  void logCall(int indir, unsigned insid, short narg, void* func, va_list args);
  void logRet(int indir, unsigned insid, short narg, void* func, uint64_t data);
  void logSync(unsigned insid, unsigned short sync,
               unsigned turn, bool after = true, ...);

  Logger(int tid);
  ~Logger();

  static __thread Logger* the; /// pointer to per-thread logger

protected:

  void mapLogTrunk(void);
  void checkAndGrowLogSize(void) {
    // TODO: check log buffer size and allocate new space if necessary
    assert(off + RECORD_SIZE <= TRUNK_SIZE);
  }
  void checkAndSetInsid(unsigned &insid) {
    if(insid == INVALID_INSID)
      insid &= (1<<REC_TYPE_BITS)-1;
    assert(insid < MAX_INSID && "instruction id larger than (1U<<29)!");
  }

  char*      buf;
  unsigned   off;
  int        fd;
  off_t      foff;
  char       logFile[64];

  /// code and data shared by all loggers
public:
  static void markLoggableCallee(void *func, unsigned funcid, const char* name) {
    loggableCallees[func] = funcid;
  }
  static void markEscapeCallee(void *func, unsigned funcid, const char* name) {
    escapeCallees[func] = funcid;
  }
  static bool isLoggableCallee(void *func) {
    return loggableCallees.find(func) != loggableCallees.end();
  }
  static bool isEscapeCallee(void *func) {
    return escapeCallees.find(func) != escapeCallees.end();
  }

  typedef std::tr1::unordered_map<void*, unsigned> func_map;
  static func_map loggableCallees;
  static func_map escapeCallees;

  static void progBegin();
  static void progEnd();
  static void threadBegin(int tid);
  static void threadEnd(void);

};

}

#endif
