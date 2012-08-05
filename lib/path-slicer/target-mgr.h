#ifndef __TERN_PATH_SLICER_TARGET_MANAGER_H
#define __TERN_PATH_SLICER_TARGET_MANAGER_H

#include "llvm/ADT/DenseSet.h"
#include "klee/Checker.h"
#include "dyn-instrs.h"
#include "stat.h"

namespace tern {
  class TargetMgr {
  private:
    Stat *stat;
  	
    /* Map from path (state) address to a set ot dynamic instructions (targets). */
    llvm::DenseMap<void *, llvm::DenseSet<DynInstr *> *> targets;

    /* Map from a dynamic instruction (target) to its argument mask. We leave this mask information
    in the target manager, not in the dyn-instr class, because this information is target specific. */
    llvm::DenseMap<DynInstr *, klee::Checker::ResultType> targetMasks;
    
  protected:
    bool hasTarget(void *pathId);
    void setTargetMask(DynInstr *dynInstr, klee::Checker::ResultType mask);

  public:
    TargetMgr();
    ~TargetMgr();
    void init(Stat *stat);
    void markTarget(void *pathId, DynInstr *dynInstr, uchar reason, klee::Checker :: ResultType mask);
    void copyTargets(void *newPathId,void *curPathId);
    void clearTargets(void *pathId);
    klee::Checker::ResultType getTargetMask(DynInstr *dynInstr);
    void printTargets(void *pathId, const char *tag);
    size_t sizeOfTargets(void *pathId);
    // There could be other functionalities to be added when there are more checkers.
  };
}

#endif

