#ifndef __TERN_PATH_SLICER_H
#define __TERN_PATH_SLICER_H

#include <list>

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "stat.h"
#include "dyn-instr.h"
#include "slice.h"
#include "instr-region.h"
#include "callstack-mgr.h"
#include "inter-slicer.h"
#include "intra-slicer.h"

namespace tern {
  struct PathSlicer: public llvm::ModulePass {
  private:
    Stat stat;
    std::list<DynInstr *> trace;
    InstrRegions instrRegions;
    CallStackMgr *ctxMgr;
    
    InterSlicer interSlicer;
    IntraSlicer intraSlicer;

  protected:
    /* Must look at the xtern trace format before implementing this function. */
    void loadTrace();

    /* Enforce racy edge and split new instruction regions.
    A challenge is if we enforce partial order of racy edge, how to adjust the logical clock 
    for these regions, given the logical clocks are based on start/end totol order of synch op index. */
    void enforceRacyEdges();
    void calStat();
    
  public:
    static char ID;
    PathSlicer();
    virtual ~PathSlicer();
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    void runPathSlicer(llvm::Module &M);
  };
}

#endif

