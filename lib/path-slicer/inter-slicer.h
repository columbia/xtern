#ifndef __TERN_PATH_SLICER_INTER_SLICER_H
#define __TERN_PATH_SLICER_INTER_SLICER_H

#include "llvm/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "stat.h"
#include "dyn-instrs.h"
#include "rw-set.h"
#include "instr-region.h"
#include "slice.h"
#include "alias-mgr.h"
#include "cfg-mgr.h"

namespace tern {
  struct PathSlicer;
  struct InterSlicer {
  private:
    PathSlicer *pathSlicer;
    Stat *stat;
    RWSet rwSet;
    Slice *targets;
    Slice *slice;
    AliasMgr *aliasMgr;
    CfgMgr *cfgMgr;

  protected:
    void detectInstrInstrRaces(InstrRegion *fromRegion, InstrRegion *toRegion);
    void detectBrInstRaces(InstrRegion *fromRegion, InstrRegion *toRegion);
    void detectBrBrRaces(InstrRegion *fromRegion, InstrRegion *toRegion);
    bool instRaceWithRegion(DynInstr *iInfo, InstrRegion *r);
    bool brRaceWithReadSet(DynInstr *iInfo, llvm::BasicBlock *bb, RWSet &readSet);
    bool brRaceWithWriteSet(DynInstr *iInfo, llvm::BasicBlock *bb, RWSet &writeSet);
    bool brRaceWithRegion(DynInstr *iInfo, llvm::BasicBlock *bb, InstrRegion *r);
    void addToTargets(InstrRegion *region, DynInstr *iInfo, const char *reason);
    void addLandmarksToTargets(InstrRegion *region);
        

  public:
    InterSlicer();
    ~InterSlicer();
    void detectInputDepRaces(InstrRegions *instrRegions);
    void calStat();
  };
}

#endif

