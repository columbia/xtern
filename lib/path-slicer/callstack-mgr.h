#ifndef __TERN_PATH_SLICER_CALLSTACK_MGR_H
#define __TERN_PATH_SLICER_CALLSTACK_MGR_H

#include <vector>

#include "dyn-instrs.h"
#include "instr-id-mgr.h"
#include "stat.h"
#include "func-summ.h"
#include "target-mgr.h"

namespace tern {
  class CallStackMgr {
  private:
    FuncSumm *funcSumm;
    Stat *stat;
    InstrIdMgr *idMgr;
    TargetMgr *tgtMgr;
    
    /* Map from thread id to a sequence of current active dynamic call instructions. */
    llvm::DenseMap<int, std::vector<DynCallInstr *> *> tidToCallSeqMap;

    /* Map from thread id to current calling context. For any instructions, just copy from current pointer.  */
    llvm::DenseMap<int, std::vector<int> *> tidToCurCtxMap;

    /* Pools of allocated calling context, map from the length to a set of ctx with this length.
    In normal slicing mode, this is the normal calling context; in max and range slicing mode, this
    is the max calling context. Not sure how to construct simplified calling context yet. */
    llvm::DenseMap<int, llvm::DenseSet<std::vector<int> * > *> ctxPool;

    llvm::DenseMap<int, llvm::DenseSet<std::vector<int> * > *> simCtxPool;

  protected:
    void printTidToCallStackMap();

  public:
    CallStackMgr();
    ~CallStackMgr();
    void clear();
    void init(Stat *stat, InstrIdMgr *idMgr, FuncSumm *funcSumm, TargetMgr *tgtMgr);
    
    /* For normal slicing mode, this is the orig call stack;
        for mx slicing mode, this is the mx call stack. */
    void setCallStack(DynInstr *dynInstr);
    
    DynCallInstr *getCaller(DynInstr *dynInstr);

    /* For each loaded dynamic instruction, update the per-thread "current" call stack. */
    void updateCallStack(DynInstr *dynInstr);
    void printCallStack(DynInstr *dynInstr);
  };
}

#endif

