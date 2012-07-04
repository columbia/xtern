#ifndef __TERN_PATH_SLICER_FUNC_SUMMARY_H
#define __TERN_PATH_SLICER_FUNC_SUMMARY_H

#include "llvm/Instruction.h"
#include "llvm/Function.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Pass.h"

#include "common/callgraph-fp.h"

#include "event-mgr.h"
#include "dyn-instrs.h"
#include "cache-util.h"

namespace tern {
  // TBD: should move the internal function list from path slicer to here.

  /* This class stores summary of all functions within a module:
    (1) Whether an the address pointed to by a input argument will be overwritten by
        the external function call.
    (2) Whether a function is internal (in the orig module, not including linking of uclibc).
    ...
  */
  class FuncSumm: public llvm::FunctionPass {
  public:
    static char ID;

  private:
    InstrIdMgr *idMgr;
    EventMgr *EM;

#if 0    
    /* Set of events such as lock()/unlock(), or fopen()/fclose(). */
    llvm::DenseSet<const llvm::Function *> events;
    /* Set of all functions that may reach any events in the function body. */
    llvm::DenseSet<const llvm::Function *> eventFunctions;
#endif

    /* Set of all internal functions from original bc module (regardless of uclibc). */
    llvm::DenseSet<const llvm::Function *> internalFunctions;

    llvm::Function *mainFunc;

    CacheUtil extLoadCache;
    CacheUtil extStoreCache;

  protected:

  public:
    FuncSumm();
    ~FuncSumm();

    void init(InstrIdMgr *idMgr, EventMgr *EM);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnFunction(Function &F);

    /* Event functions. */
    /* The initEvents() must be called before run() on this func-summ, since event-func will
    be run before func-summ is run. */
    bool mayCallEvent(const llvm::Function *f);
    bool mayCallEvent(DynInstr *dynInstr);
    bool eventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr);

    /* Since uclibc would be linked in, some functions such as memcpy() would become internal
    after this linking. But we only care about "guest" LLVM code in slicing. So, these functions are
    the only places within the slicing system to determine which functions are internal or not. */

    /* Note that there is a huge challenge here: after uclibc is linked in, most external functions in
    the original bc would be deleted (so their pointers are gone) and relinked with functions in uclibc.
    For instance, fwrite() -> fwrite_unlocked(). But there is no problem, because all internal functions
    are still there, the pointers are not changed (although some names would be changed, e.g.,
    __user_main). When we are using callgraph-fp, or function summary, must pay attention to the
    removed and relinked "external" functions.
    Of couse, this feature might bring troubles for me writing external function summaries.
    */
    bool isInternalFunction(const llvm::Function *f);
    bool isInternalCall(const llvm::Instruction *instr);
    bool isInternalCall(DynInstr *dynInstr);


    /* External function summary. Ignore function pointer for now. Must handle bitcast for now. */
    /* Given a static instruction (must be a function call), whether it has load/store summary. */
    bool extFuncHasLoadSumm(llvm::Instruction *instr);
    bool extFuncHasStoreSumm(llvm::Instruction *instr);
    bool extFuncHasSumm(llvm::Instruction *instr);

    /* Given a static instruction (must be a function call) and the formal argument offset,
    whether the argument has load/store summary. */
    bool isExtFuncSummLoad(llvm::Instruction *instr, unsigned argOffset);
    bool isExtFuncSummStore(llvm::Instruction *instr, unsigned argOffset);

	/* Whether an instruction calls to an event. */
    bool isEventCall(DynCallInstr *callInstr);
    size_t numEventCallSites();
    void printEventCalls();

  };

}

#endif

