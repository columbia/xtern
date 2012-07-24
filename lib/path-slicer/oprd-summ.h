#ifndef __TERN_PATH_SLICER_OPRD_SUMMARY_H
#define __TERN_PATH_SLICER_OPRD_SUMMARY_H

#include "llvm/Instruction.h"
#include "llvm/BasicBlock.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "bc2bdd/ext/JavaBDD/buddy/src/bdd.h"
#include "common/callgraph-fp.h"

#include "dyn-instrs.h"
#include "type-defs.h"
#include "cache-util.h"
#include "stat.h"
#include "func-summ.h"
#include "alias-mgr.h"
#include "instr-id-mgr.h"

namespace tern {
  /* This class is used to collect not-executed alias information.

  This class collects both stored pointer operands as well as their bdd, not just bdd,
  because some analysis (e.g., the range analysis) require operands.

  In inter-thread phase, it is used to collect stored pointer operands and their bdd 
  from not-executed branches to see potential branch-branch races.

  In intra-thread phase, it is used to collect stored pointer operands and their bdd
  from writtenBetween() when checking not-executed branches, and mayWriteFunc()
  for return instructions. */
  class OprdSumm: public llvm::ModulePass {
  private:
    enum OprdType {Load = 1, Store = 2, PHI = 3};
    static char ID;
    Stat *stat;
    AliasMgr *aliasMgr;
    InstrIdMgr *idMgr;
    FuncSumm *funcSumm;
    llvm::CallGraphFP *CG;

    /* All "reachable" store pointer operands. This is collected by a inter-procedural algorithm. */
    /* Used by mayWriteFunc() in intra-thread phase. */
    llvm::DenseMap<llvm::Function *, InstrDenseSet * > funcStoreSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::Function *>, bdd * > funcStoreBDD;
    
    /* Used by writtenBetween() in intra-thread phase and rw-set in inter-thread phase. */
    llvm::DenseMap<llvm::BasicBlock *, InstrDenseSet * > bbStoreSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::BasicBlock *>, bdd * > bbStoreBDD;

    /* All "reachable" load pointer operands. This is collected by a inter-procedural algorithm. */
    /* It is not directly used by other modules, but it is used when collecting inter-procedural
    load summary for bbLoadSumm. */
    llvm::DenseMap<llvm::Function *, InstrDenseSet * > funcLoadSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::Function *>, bdd * > funcLoadBDD;

    /* Used by rw-set in inter-thread phase (branch-branch may race). */
    llvm::DenseMap<llvm::BasicBlock *, InstrDenseSet * > bbLoadSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::BasicBlock *>, bdd * > bbLoadBDD;

    /* Used by phiDefBetween() in intra-thread phase. Maps from each basicblock
    to a set of phi instructions in anywhere the bitcode, if a phi instruction uses any
    virtual registers defined in this bb. Intra-procedural. */
    llvm::DenseMap<llvm::BasicBlock *, InstrDenseSet * > bbPhiDefSumm;

    /* Map from a pair of <prevInstr, postInstr> to a set of phi instructions,
    if a phi instruction uses any virtual registers defined in between of these
    two instructions. Intra-procedural. */
    //llvm::DenseMap<PtrPair, ValueDenseSet *> phiDefSummCache;

    DenseSet<llvm::Function *> visited;
    DenseSet<llvm::BasicBlock *> visitedBB;

    llvm::Function *mainFunc;

  protected:
    void initAllSumm(llvm::Module &M);

    /* Phase 1: compute store pointer operands for a function (locally).
      The summary of callees are ignores in this phase. */
    void collectSummLocal(llvm::Module &M);
    void collectFuncSummLocal(llvm::Function *f);
    void collectInstrSummLocal(llvm::Instruction *instr);

    /* Phase 2: goes from top to bottom of call graph, append summary from callee to caller. */
    void collectSummTopDown(llvm::Module &M);
    void DFSTopDown(llvm::Function *f);
    void addSummToCallerTopDown(llvm::Function *callee,
      llvm::Function *caller, Instruction *callInstr);
    void addSummTopDown(InstrDenseSet *calleeSet, InstrDenseSet *callerSet);

    /* Functions used during outside query. */
    void DFSBasicBlock(llvm::BasicBlock *x, llvm::BasicBlock *sink,
      InstrDenseSet &summ, OprdType oprdType);

    void printSumm(InstrDenseSet &summ, const char *tag);

  public:
    OprdSumm();
    ~OprdSumm();
    void init(Stat *stat, FuncSumm *funcSumm, AliasMgr *aliasMgr, InstrIdMgr *idMgr, llvm::CallGraphFP *CG);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);

    /* Given a prev dynamic instruction and its post dominator (another dynamic instruction),
    calculate all reachable load instructions and their bdd. The load instructions are those in 
    corespoing module (normal, mx, or sim). Used in inter-thread phase. */
    InstrDenseSet *getLoadSummBetween(DynInstr *prevInstr, 
      DynInstr *postInstr, bdd &bddResults);

    /* Given a prev dynamic instruction and its post dominator (another dynamic instruction),
    calculate all reachable store instructions and their bdd. The store instructions are those in 
     corespoing module (normal, mx, or sim). We only go over the not-executed successor. */
    InstrDenseSet *getStoreSummBetween(DynBrInstr *prevInstr,
      DynInstr *postInstr, bdd &bddResults);

    /* Given a dynamic return instruction, calculate all reachable store instructions within the
    called function and their bdd. The store instructions are those in corespoing module 
    (normal, mx, or sim).*/
    InstrDenseSet *getStoreSummInFunc(
      DynInstr *dynInstr, DynCallInstr *caller, bdd &bddResults);

    /* Given a dynamic call instruction, return a bdd to specify all the "ExtStore" summary of
    that instruction.*/
    InstrDenseSet *getExtCallStoreSumm(DynCallInstr *callInstr, bdd &bddResults);

    /* Return  a set of phi instructions in anywhere the bitcode, if a phi instruction uses any
    virtual registers defined in between (of the prevBrInstr and postInstr). Intra-procedural. */
    void getUsedByPhiSummBetween(DynBrInstr *prevBrInstr, DynInstr *postInstr, InstrDenseSet &phiSet);
  };
}

#endif

