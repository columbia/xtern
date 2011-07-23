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
  void logLoad (unsigned insid, char* addr, uint64_t data);
  void logStore(unsigned insid, char* addr, uint64_t data);
  void logCall(uint8_t flags, unsigned insid,
               short narg, void* func, va_list args);
  void logRet(uint8_t flags, unsigned insid,
              short narg, void* func, uint64_t data);
  void logSync(unsigned insid, unsigned short sync,
               unsigned turn, bool after = true, ...);

  Logger(const char* filename);
  ~Logger();

  static __thread Logger* the; /// pointer to per-thread logger

protected:

  void mapLogTrunk(void);
  void checkAndGrowLogSize(void) {
    // TODO: check log buffer size and allocate new space if necessary
    assert(off + RECORD_SIZE <= TRUNK_SIZE);
  }

  char*      buf;
  unsigned   off;
  int        fd;
  off_t      foff;
  char       logFile[64];

  /// code and data shared by all loggers
public:
  static void markFuncCallLogged(void *func, unsigned funcid, const char* name){
    funcsCallLogged[func] = funcid;
  }
  static void markFuncEscape(void *func, unsigned funcid, const char* name) {
    funcsEscape[func] = funcid;
  }
  static bool funcCallLogged(void *func) {
    return funcsCallLogged.find(func) != funcsCallLogged.end();
  }
  static bool funcEscape(void *func) {
    return funcsEscape.find(func) != funcsEscape.end();
  }

  typedef std::tr1::unordered_map<void*, unsigned> func_map;
  static func_map funcsCallLogged;
  static func_map funcsEscape;

  static void progBegin();
  static void progEnd();
  static void threadBegin(const char* filename);
  static void threadEnd(void);

};

}

#endif
