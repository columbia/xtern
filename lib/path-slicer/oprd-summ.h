#ifndef __TERN_PATH_SLICER_OPRD_SUMMARY_H
#define __TERN_PATH_SLICER_OPRD_SUMMARY_H

#include "llvm/Instruction.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "bc2bdd/ext/JavaBDD/buddy/src/bdd.h"

#include "dyn-instrs.h"
#include "type-defs.h"
#include "cache-util.h"
#include "stat.h"
#include "func-summ.h"

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
    static char ID;
    Stat *stat;

    FuncSumm *funcSumm;

    /* All "reachable" store pointer operands. This is collected by a inter-procedural algorithm. */
    /* Used by mayWriteFunc() in intra-thread phase. */
    llvm::DenseMap<const llvm::Function *, InstrDenseSet * > funcStoreSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::Function *>, bdd * > funcStoreBDD;
    
    /* Used by writtenBetween() in intra-thread phase and rw-set in inter-thread phase. */
    llvm::DenseMap<const llvm::BasicBlock *, InstrDenseSet * > bbStoreSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::BasicBlock *>, bdd * > bbStoreBDD;

    /* All "reachable" load pointer operands. This is collected by a inter-procedural algorithm. */
    /* It is not directly used by other modules, but it is used when collecting inter-procedural
    load summary for bbLoadSumm. */
    llvm::DenseMap<const llvm::Function *, InstrDenseSet * > funcLoadSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::Function *>, bdd * > funcLoadBDD;

    /* Used by rw-set in inter-thread phase (branch-branch may race). */
    llvm::DenseMap<const llvm::BasicBlock *, InstrDenseSet * > bbLoadSumm;
    /* cache */
    //llvm::DenseMap<std::pair<CallCtx *, const llvm::BasicBlock *>, bdd * > bbLoadBDD;

    DenseSet<const Function *> visited;

  protected:
    void clean();
    void initAllSumm(llvm::Module &M);

    /* Phase 1: compute store pointer operands for a function (locally).
      The summary of callees are ignores in this phase. */
    void collectSummLocal(llvm::Module &M);
    void collectFuncSummLocal(const llvm::Function *f);
    void collectInstrSummLocal(const llvm::Instruction *instr);

    /* Phase 2: goes from top to bottom of call graph, append summary from callee to caller. */
    void collectSummTopDown(llvm::Module &M);
    void DFSTopDown(const llvm::Function *f);
    void addSummToCallerTopDown(const llvm::Function *callee,
      const llvm::Function *caller, const Instruction *callInstr);
    void addSummTopDown(InstrDenseSet *calleeSet, InstrDenseSet *callerSet);

  public:
    OprdSumm();
    ~OprdSumm();
    void initStat(Stat *stat);
    void initFuncSumm(FuncSumm *funcSumm);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);

    /* Given a prev dynamic instruction and its post dominator (another dynamic instruction),
    calculate all reachable load instructions and their bdd. The load instructions are those in 
    corespoing module (normal, mx, or sim). */
    llvm::DenseSet<llvm::Instruction *> *getLoadSummBetween(
      DynInstr *prevInstr, DynInstr *postInstr, bdd &bddResults);

    /* Given a prev dynamic instruction and its post dominator (another dynamic instruction),
    calculate all reachable store instructions and their bdd. The store instructions are those in 
    corespoing module (normal, mx, or sim). */
    llvm::DenseSet<llvm::Instruction *> *getStoreSummBetween(
      DynInstr *prevInstr, DynInstr *postInstr, bdd &bddResults);

    /* Given a dynamic call instruction, calculate all reachable store instructions within the
    function and their bdd. The store instructions are those in corespoing module 
    (normal, mx, or sim).*/
    llvm::DenseSet<llvm::Instruction *> *getStoreSummInFunc(
      DynCallInstr *callInstr, bdd &bddResults);
  };
}

#endif

