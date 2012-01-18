#ifndef __TERN_PATH_SLICER_DYN_OPRD_H
#define __TERN_PATH_SLICER_DYN_OPRD_H

#include "dyn-instrs.h"

#include "llvm/Instruction.h"

namespace tern {
  class DynInstr;
  /* This class is no longer involved in heap, it is only performed in function local computation,
  so memory usage is no longer a problem. */
  class DynOprd {
  private:
    /* Points back to the dynamic instruction that owns this dynamic operand. */
    DynInstr *dynInstr;

    /* Start from -1. */
    int oprdIndex; 

  protected:

  public:
    DynOprd();
    DynOprd(DynInstr *dynInstr, int oprdIndex);
    ~DynOprd();
    int getIndex();
    DynInstr *getDynInstr();
    llvm::Value *getStaticValue();
    bool isConstant();
  };
}

#endif

