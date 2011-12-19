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
    
  public:
    static char ID;
    PathSlicer();
    virtual ~PathSlicer();
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    void loadTrace();
    void enforceRacyEdges();
    void runPathSlicer(llvm::Module &M);
    void calStat();
  };
}

#endif

