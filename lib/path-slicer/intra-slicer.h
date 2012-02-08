#ifndef __TERN_PATH_SLICER_INTRA_SLICER_H
#define __TERN_PATH_SLICER_INTRA_SLICER_H

#include <list>

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "klee/ExecutionState.h"

#include "stat.h"
#include "dyn-instrs.h"
#include "slice.h"
#include "live-set.h"
#include "instr-id-mgr.h"
#include "alias-mgr.h"
#include "cfg-mgr.h"
#include "func-summ.h"
#include "oprd-summ.h"

namespace tern {
  /* Note:
    Current slicing algorithm which suits for PEREGRINE ignores "values", which would be required by 
    the directed symbolic execution project.
  */
  struct IntraSlicer {
  private:
    klee::ExecutionState *state;
    OprdSumm *oprdSumm;
    FuncSumm *funcSumm;
    Stat *stat;
    LiveSet live;
    Slice slice;
    InstrIdMgr *idMgr;
    AliasMgr *aliasMgr;
    CfgMgr *cfgMgr;
    const DynInstrVector *trace;
    size_t curIndex;

  protected:
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
    DynInstr *delTraceTail(uchar tid);

    void takeNonMem(DynInstr *dynInstr, uchar reason = INTRA_NON_MEM);
    void delRegOverWritten(DynInstr *dynInstr);
    bool regOverWritten(DynInstr *dynInstr);
    bool retRegOverWritten(DynInstr *dynInstr);
    bool eventBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr);
    bool writtenBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr);
    bool mayWriteFunc(DynRetInstr *dynRetInstr, llvm::Function *func);
    bool mayCallEvent(DynInstr *dynInstr, llvm::Function *func);
    DynInstr *getCallInstrWithRet(DynInstr *retDynInstr);

    /* Find previous dynamic instruction in the trace with the same thread id. */
    DynInstr *prevDynInstr(DynInstr *dynInstr);

    /* Make curIndex points to the previous instruction of the call instruction of the ret instruction. */
    void removeRange(DynRetInstr *dynRetInstr);
    void addMemAddrEqConstr(DynMemInstr *loadInstr, DynMemInstr *storeInstr);
    bool mustAlias(DynOprd *oprd1, DynOprd *oprd2);

  public:
    IntraSlicer();
    ~IntraSlicer();
    void init(klee::ExecutionState *state, OprdSumm *oprdSumm, FuncSumm *funcSumm,
      InstrIdMgr *idMgr, const DynInstrVector *trace, size_t startIndex);

    /* The function to run the intra-thread slicing. */
    void detectInputDepRaces(uchar tid);

    /* The function to initially take a instruction for path-slicer (for reachability only). */
    void takeStartTarget(DynInstr *dynInstr);

    /* This function could be called by the path-slicer to setup init dyn oprds to live set,
    depending on different slicing goals (reachability only, or reachability + values of used oprds). */
    void addDynOprd(DynOprd *dynOprd);
  };
}

#endif

