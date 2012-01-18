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

  protected:

  public:
    OprdSumm();
    ~OprdSumm();
    void initStat(Stat *stat);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);

    /* Given a prev dynamic instruction and its post dominator (another dynamic instruction),
    calculate all reachable store instructions and their bdd. The store instructions are those in 
    corespoing module (normal, mx, or sim). */
    std::set<llvm::Instruction *> *getStoreSummBetween(
      DynInstr *prevInstr, DynInstr *postInstr, bdd &bddResults);

    /* Given a dynamic call instruction, calculate all reachable store instructions within the
    function and their bdd. The store instructions are those in corespoing module 
    (normal, mx, or sim).*/
    std::set<llvm::Instruction *> *getStoreSummInFunc(
      DynCallInstr *callInstr, bdd &bddResults);
  };
}

#endif

