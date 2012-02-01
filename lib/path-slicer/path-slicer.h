#ifndef __TERN_PATH_SLICER_H
#define __TERN_PATH_SLICER_H

#include <list>

#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "klee/Internal/Module/KModule.h"

#include "stat.h"
#include "dyn-instrs.h"
#include "slice.h"
#include "instr-region.h"
#include "callstack-mgr.h"
#include "inter-slicer.h"
#include "intra-slicer.h"
#include "cfg-mgr.h"
#include "klee-trace-util.h"
#include "xtern-trace-util.h"
#include "oprd-summ.h"

namespace tern {
  struct PathSlicer: public llvm::ModulePass {
  private:
    /* Statistics information of path slicer. */
    Stat stat;

    /* The generic recording and loading interface of a trace. */
    TraceUtil *traceUtil;

    /* Use vector rather than list here becuase we usually needs to start slicing from some index
    of the trace, and this vector is stable once trace is loaded. */
    llvm::DenseMap<void *, DynInstrVector *> allPathTraces;

    /* Slicing targets, can come from inter-thread phase or specified by users manually. */
    DynInstrList targets;

    /* InstrRegion(s) is only for inter-thread phase. */
    InstrRegions instrRegions;

    /* Inter-thread slicer. */
    InterSlicer interSlicer;

    /* Intra-thread slicer. */
    IntraSlicer intraSlicer;

    /* The whole system only has this copy of calling context manager, all the others are pointers.
    It uses idMgr. It does not invole any LLVM modules. */
    CallStackMgr ctxMgr;

    /* Alias manager. It uses idMgr. It involves all the three
    (orig, mx and simplified) LLVM modules. */
    AliasMgr aliasMgr;

    /* Max slicing ID manager. It involves all the three
    (orig, mx and simplified) LLVM modules. */
    InstrIdMgr idMgr;

    /* CFG manager. It only involves the orig LLVM module. */
    CfgMgr cfgMgr;

    /* Oprd summary. It uses idMgr. It involves all the three
    (orig, mx and simplified) LLVM modules. */
    OprdSumm oprdSumm;

    /* Since KLEE will link in uclibc library, so some functions in the module may become
    internal such as memcpy() after this linking. Our slicing only focus on guest program,
    so we have to collect the set of all internal functions before uclibc is linked in. */
    llvm::DenseSet<const llvm::Function *> internalFunctions;

  protected:
    /* Init all sub-components of the path-slicer. */
    void init(llvm::Module &M);

    /* Enforce racy edge and split new instruction regions.
    A challenge is if we enforce partial order of racy edge, how to adjust the logical clock 
    for these regions, given the logical clocks are based on start/end totol order of synch op index. */
    void enforceRacyEdges();
    void calStat();
    llvm::Module *loadModule(const char *path);
    void collectInternalFunctions(llvm::Module &M);
    void freeCurPathTrace(void *pathId);
    
  public:
        static char ID;

  public:
    PathSlicer();
    virtual ~PathSlicer();
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    void initKModule(klee::KModule *kmodule);

    /* The uniformed recording interface to record an in-memory execution trace. */
    void record(void *pathId, void *instr, void *state, void *f);


    /* The key function called by other modules (such as KLEE) to get relevant branches. */
    void runPathSlicer(void *pathId, std::set<llvm::BranchInst *> &brInstrs);

    /* Since uclibc would be linked in, some functions such as memcpy() would become internal
    after this linking. But we only care about "guest" LLVM code in slicing. So, these functions are
    the only places within the slicing system to determine which functions are internal or not. */
    bool isInternalFunction(const llvm::Function *f);
    bool isInternalCall(const llvm::Instruction *instr);
    bool isInternalCall(DynInstr *dynInstr);
  };
}

#endif

