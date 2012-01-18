#ifndef __TERN_PATH_SLICER_INTRA_SLICER_H
#define __TERN_PATH_SLICER_INTRA_SLICER_H

#include <list>

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "stat.h"
#include "dyn-instrs.h"
#include "slice.h"
#include "live-set.h"
#include "alias-mgr.h"
#include "cfg-mgr.h"

namespace tern {
  /* Note:
    Current slicing algorithm which suits for PEREGRINE ignores "values", which would be required by 
    the directed symbolic execution project.
  */
  struct IntraSlicer {
  private:
    Stat *stat;
    LiveSet live;
    Slice *slice;
    AliasMgr *aliasMgr;
    CfgMgr *cfgMgr;
    const DynInstrVector *trace;
    size_t curIndex;

  protected:
    void take(LiveSet &live, Slice &slice, DynInstr *dynInstr, const char *reason);
    bool mayWrite(DynInstr *dynInstr, LiveSet &live);
    bool mayWriteFunc(DynInstr *dynRetInstr, llvm::Function *func, LiveSet &live);
    bool writtenBetween(DynInstr *dynInstr, DynInstr *dynHeadInstr, LiveSet &live);
    llvm::Instruction *getStaticPostDominator(DynInstr *dynInstr);
    bool postDominate(DynInstr *dynPostInstr, DynInstr *dynPrevInstr);
    DynInstrItr current();
    DynInstr *delFromTrace();
    void handlePHI(DynInstr *dynInstr);
    void handleBranch(DynInstr *dynInstr);
    void handleRet(DynInstr *dynInstr);
    void handleCall(DynInstr *dynInstr);
    void handleNonMem(DynInstr *dynInstr);
    void handleMem(DynInstr *dynInstr);
    bool empty();
    DynInstr *delTraceTail();
    void takeNonMem(DynInstr *dynInstr);
    void delRegOverWritten(DynInstr *dynInstr);
    bool regOverWritten(DynInstr *dynInstr);
    bool retRegOverWritten(DynInstr *dynInstr);
    bool eventBetween(DynInstr *dynInstr);
    bool writtenBetween(DynInstr *dynInstr);
    bool mayWriteFunc(DynInstr *dynInstr, llvm::Function *func);
    bool mayCallEvent(DynInstr *dynInstr, llvm::Function *func);
    DynInstr *getCallInstrWithRet(DynInstr *retDynInstr);
    DynInstr *prevDynInstr(DynInstr *dynInstr);
    void removeRange(DynInstr *dynInstr);
    void addMemAddrEqConstr(DynMemInstr *loadInstr, DynMemInstr *storeInstr);

  public:
    IntraSlicer();
    ~IntraSlicer();
    void init(const DynInstrVector *trace, size_t startIndex);
    void detectInputDepRaces();
  };
}

#endif

