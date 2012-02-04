#ifndef __TERN_PATH_SLICER_FUNC_SUMMARY_H
#define __TERN_PATH_SLICER_FUNC_SUMMARY_H

#include "llvm/Instruction.h"
#include "llvm/Function.h"
#include "llvm/ADT/DenseSet.h"

#include "dyn-instrs.h"

namespace tern {
  // TBD: should move the internal function list from path slicer to here.

  /* This class stores summary of all functions within a module:
    (1) Whether an the address pointed to by a input argument will be overwritten by
        the external function call.
    (2) Whether a function is internal (in the orig module, not including linking of uclibc).
    ...
  */
  class FuncSumm {
  private:
    llvm::DenseSet<const llvm::Function *> internalFunctions;

  protected:

  public:
    FuncSumm();
    ~FuncSumm();

    bool isInternalFunction(const llvm::Function *f);
    bool isInternalCall(const llvm::Instruction *instr);
    bool isInternalCall(DynInstr *dynInstr);
  };

}

#endif

