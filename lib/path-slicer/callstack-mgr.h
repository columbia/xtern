#ifndef __TERN_PATH_SLICER_CALLSTACK_MGR_H
#define __TERN_PATH_SLICER_CALLSTACK_MGR_H

#include <vector>

#include "dyn-instrs.h"
#include "instr-id-mgr.h"

namespace tern {
  struct CallSitePnt {
    int callSiteIid;
    Function *calledFunc;
  };
  class CallStackMgr {
  private:
    InstrIdMgr *idMgr;
    llvm::DenseMap<int, vector<CallSitePnt *> *> tidToCallStackMap;
    std::set<vector<int> *> ctxPool;
    llvm::DenseMap<int, vector<CallSitePnt *> *> tidToMxCallStackMap;
    std::set<vector<int> *> mxCtxPool;
    llvm::DenseMap<int, vector<CallSitePnt *> *> tidToSimCallStackMap;
    std::set<vector<int> *> simCtxPool;

  protected:
    void printCallStack(DynInstr *dynInstr);
    void printTidToCallStackMap();

    /* For each loaded dynamic instruction, update the per-thread "current" call stack. */
    void updateCallStack(DynInstr *dynInstr);

    /*  */
    void setSimCallStack(DynInstr *dynInstr);

  public:
    CallStackMgr();
    ~CallStackMgr();

    /* For normal slicing mode, this is the orig call stack; for mx slicing mode, this is the mx call stack. */
    void setCallStack(DynInstr *dynInstr);
  };
}

#endif

