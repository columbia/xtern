#ifndef __TERN_PATH_SLICER_CHECKER_MANAGER_H
#define __TERN_PATH_SLICER_CHECKER_MANAGER_H

#include "llvm/ADT/DenseSet.h"

#include "dyn-instrs.h"

namespace tern {
  class CheckerMgr {
  private:
    size_t largestIdx;
    llvm::DenseSet<DynInstr *> checkerTargets;
    
  protected:

  public:
    CheckerMgr();
    ~CheckerMgr();
    void markCheckerTarget(DynInstr *dynInstr);
    void markCheckerTargetOfCall(DynCallInstr *dynCallInstr);
    size_t getLargestCheckerIdx();
    bool hasTarget();
    // There could be other functionalities to be added when there are more checkers.
  };
}

#endif

