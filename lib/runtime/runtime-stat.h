#ifndef __TERN_COMMON_RUNTIME_STAT_H
#define __TERN_COMMON_RUNTIME_STAT_H

#include <stdio.h>
#include <iostream>

namespace tern {
class RuntimeStat {
public:
  long nDetPthreadSyncOp;
  long nInterProcSyncOp;
  long nLineupSucc;
  long nLineupTimeout;
  long nNonDetRegions;
  long nNonDetPthreadSync;
  
public:
  RuntimeStat() {
    nDetPthreadSyncOp = 0;
    nInterProcSyncOp = 0;
    nLineupSucc = 0;
    nLineupTimeout = 0;
    nNonDetRegions = 0;
    nNonDetPthreadSync = 0;    
  }
  void print() {
    std::cout << "\n\nRuntimeStat:\n"
      << "nDetPthreadSyncOp\t" << "nInterProcSyncOp\t" << "nLineupSucc\t" << "nLineupTimeout\t" << "nNonDetRegions\t" << "nNonDetPthreadSync\t" << "\n"    
      << "RUNTIME_STAT: "
      << nDetPthreadSyncOp << "\t" << nInterProcSyncOp << "\t" << nLineupSucc << "\t" << nLineupTimeout << "\t" << nNonDetRegions << "\t" << nNonDetPthreadSync
      << "\n\n";
  }

};
}

#endif

