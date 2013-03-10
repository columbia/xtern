#ifndef __TERN_COMMON_RUNTIME_STAT_H
#define __TERN_COMMON_RUNTIME_STAT_H

#include <stdio.h>
#include <iostream>

namespace tern {
class RuntimeStat {
public:
  long nDetPthreadSyncOp; /* Number of deterministic pthread sync operations called (excluded idle thread and non-det sync operations).*/
  long nInterProcSyncOp;/* Number of inter-process sync operations called (networks, signals, wait, fork is scheduled by us and counted as nDetPthreadSyncOp).*/
  long nLineupSucc; /* Number of successful lineup operations (if multiple threads lineup and succeed for once, count as 1). */
  long nLineupTimeout; /* Number of lineup timeouts. */
  long nNonDetRegions;  /* Number of times all threads entering the non-det regions (and exiting the regions must be the same value). */
  long nNonDetPthreadSync; /* Number of non-det pthread sync operations called within a non-det region. */
  
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

