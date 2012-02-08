#ifndef __TERN_PATH_SLICER_FUNC_SUMMARY_H
#define __TERN_PATH_SLICER_FUNC_SUMMARY_H

#include "llvm/Instruction.h"
#include "llvm/Function.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Pass.h"

#include "common/callgraph-fp.h"

#include "event-mgr.h"
#include "dyn-instrs.h"

namespace tern {
  // TBD: should move the internal function list from path slicer to here.

  /* This class stores summary of all functions within a module:
    (1) Whether an the address pointed to by a input argument will be overwritten by
        the external function call.
    (2) Whether a function is internal (in the orig module, not including linking of uclibc).
    ...
  */
  class FuncSumm: public llvm::ModulePass {
  public:
    static char ID;

  private:
    /* Set of events such as lock()/unlock(), or fopen()/fclose(). */
    llvm::DenseSet<const llvm::Function *> events;
    /* Set of all functions that may reach any events in the function body. */
    llvm::DenseSet<const llvm::Function *> eventFunctions;

    /* Set of all internal functions from original bc module (regardless of uclibc). */
    llvm::DenseSet<const llvm::Function *> internalFunctions;

  protected:
    void collectInternalFunctions(llvm::Module &M);

  public:
    FuncSumm();
    ~FuncSumm();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &M);

    /* Event functions. */
    /* The initEvents() must be called before run() on this func-summ, since event-func will
    be run before func-summ is run. */
    bool mayCallEvent(const llvm::Function *f);
    bool mayCallEvent(DynInstr *dynInstr);
    bool eventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr);

    /* Since uclibc would be linked in, some functions such as memcpy() would become internal
    after this linking. But we only care about "guest" LLVM code in slicing. So, these functions are
    the only places within the slicing system to determine which functions are internal or not. */
    bool isInternalFunction(const llvm::Function *f);
    bool isInternalCall(const llvm::Instruction *instr);
    bool isInternalCall(DynInstr *dynInstr);
  };

}

#endif

