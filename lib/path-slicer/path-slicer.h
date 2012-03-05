#ifndef __TERN_PATH_SLICER_H
#define __TERN_PATH_SLICER_H

#include <list>
#include <string>

#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "klee/Internal/Module/KModule.h"
#include "klee/Checker.h"

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
#include "func-summ.h"
#include "target-mgr.h"

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

    /* Function summary. */
    FuncSumm funcSumm;

    TargetMgr tgtMgr;

    bool startRecord;

  protected:
    /* Init all sub-components of the path-slicer. */
    void init(llvm::Module &M);

    /* Enforce racy edge and split new instruction regions.
    A challenge is if we enforce partial order of racy edge, how to adjust the logical clock 
    for these regions, given the logical clocks are based on start/end totol order of synch op index. */
    void enforceRacyEdges();
    void calStat(std::set<llvm::BranchInst *> &rmBrs, std::set<llvm::CallInst *> &rmCalls);
    llvm::Module *loadModule(const char *path);

    /* Filter out instructions before the first instruction in main(), such as C++ ctor and klee_range(). */
    bool getStartRecord(void *instr);
    
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
    void runPathSlicer(void *pathId, std::set<llvm::BranchInst *> &rmBrs,
      std::set<llvm::CallInst *> &rmCalls);

    /* Copy recorded instructions from current path to a new branched path. */
    void copyTrace(void *newPathId, void *curPathId);

    /* Read checker result, if it is IMPORTANT, then mark the latest recorded instruction in current
    path as CHECKER_TARGET. This "latest" assumes executor of KLEE interpretes each instruction
    atomically (once for each instruction). For each a checker error, the last recorded instruction 
    (which triggered the error) will also be marked as target. */
    void recordCheckerResult(void *pathId, klee::Checker::Result globalResult,
      klee::Checker::Result localResult);

    /*  */
    bool isInternalInstr(llvm::Instruction *instr);

    /* Get the latest recorded instruction  */
    llvm::Instruction *getLatestInstr(void *pathId);

    /* Collect path exploration stats. */
    void collectExplored(llvm::Instruction *instr);

  };
}

#endif

