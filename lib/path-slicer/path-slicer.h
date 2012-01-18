#ifndef __TERN_PATH_SLICER_H
#define __TERN_PATH_SLICER_H

#include <list>

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "stat.h"
#include "dyn-instrs.h"
#include "slice.h"
#include "instr-region.h"
#include "callstack-mgr.h"
#include "inter-slicer.h"
#include "intra-slicer.h"

namespace tern {
  struct PathSlicer: public llvm::ModulePass {
  private:
    Stat stat;

    /* Use vector rather than list here becuase we usually needs to start slicing from some index
    of the trace, and this vector is stable once trace is loaded. */
    DynInstrVector trace;

    /* Slicing targets, can come from inter-thread phase or specified by users manually. */
    DynInstrList targets;

    /* InstrRegion(s) is only for inter-thread phase. */
    InstrRegions instrRegions;

    /* The whole system only has this copy of calling context manager, all the others are pointers. */
    CallStackMgr ctxMgr;

    /* Inter-thread slicer. */
    InterSlicer interSlicer;

    /* Intra-thread slicer. */
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

