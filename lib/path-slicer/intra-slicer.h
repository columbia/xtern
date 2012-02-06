#ifndef __TERN_PATH_SLICER_INTRA_SLICER_H
#define __TERN_PATH_SLICER_INTRA_SLICER_H

#include <list>

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "klee/Solver.h"
#include "klee/ExecutionState.h"

#include "stat.h"
#include "dyn-instrs.h"
#include "slice.h"
#include "live-set.h"
#include "instr-id-mgr.h"
#include "alias-mgr.h"
#include "cfg-mgr.h"
#include "func-summ.h"

namespace tern {
  /* Note:
    Current slicing algorithm which suits for PEREGRINE ignores "values", which would be required by 
    the directed symbolic execution project.
  */
  struct IntraSlicer {
  private:
    /* We need to pass in the solver from KLEE to check whether two memory addresses of two
    load/store instructions must be always the same in the intra-slicing. The solver must be a regular
    solver, not a timing solver, for soundness of the silcing algorithm. */
    klee::Solver *solver;
    klee::ExecutionState *state;
    FuncSumm *funcSumm;
    Stat *stat;
    LiveSet live;
    Slice *slice;
    InstrIdMgr *idMgr;
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

    /* TBD: all places using this function should indicate the specific taken reason. */
    void takeNonMem(DynInstr *dynInstr, uchar reason = INTRA_NON_MEM);
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
    bool mustBeSame(klee::ref<klee::Expr> expr1, klee::ref<klee::Expr> expr2);

  public:
    IntraSlicer();
    ~IntraSlicer();
    void init(klee::ExecutionState *state, FuncSumm *funcSumm, InstrIdMgr *idMgr,
      const DynInstrVector *trace, size_t startIndex);
    void initSolver(klee::Solver *solver);
    void detectInputDepRaces();
  };
}

#endif

