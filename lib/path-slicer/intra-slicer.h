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

    /* If the checkers from KLEE mark an instruction as IMPORTANT, we handle this in this function. */
    void handleCheckerTarget(DynInstr *dynInstr);

    void takeNonMem(DynInstr *dynInstr, uchar reason = INTRA_NON_MEM);
    void delRegOverWritten(DynInstr *dynInstr);
    bool regOverWritten(DynInstr *dynInstr);
    bool retRegOverWritten(DynRetInstr *dynRetInstr);
    bool eventBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr);
    bool writtenBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr);
    bool mayWriteFunc(DynRetInstr *dynRetInstr);
    bool mayCallEvent(DynRetInstr *dynRetInstr);

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
      AliasMgr *aliasMgr, InstrIdMgr *idMgr, CfgMgr *cfgMgr, Stat *stat,
      const DynInstrVector *trace, size_t startIndex);

    /* The function to run the intra-thread slicing. */
    void detectInputDepRaces(uchar tid);

    /* The function to initially take a instruction for path-slicer (for reachability and the value of its
    used operands, do not need to care about its destination operand, even if it exists). */
    void takeStartTarget(DynInstr *dynInstr);

    void calStat();
  };
}

#endif

