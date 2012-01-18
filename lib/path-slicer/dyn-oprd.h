#ifndef __TERN_PATH_SLICER_DYN_OPRD_H
#define __TERN_PATH_SLICER_DYN_OPRD_H

#include "dyn-instrs.h"

#include "llvm/Instruction.h"

namespace tern {
  class DynInstr;
  /* We must include as few as fields in this class in order to save memory. */
  class DynOprd {
  private:
    /* Points back to the dynamic instruction that owns this dynamic operand. */
    DynInstr *dynInstr;

    /* Start from 0. */
    int oprdIndex; 

  protected:

  public:
    DynOprd();
    ~DynOprd();
    int getIndex();
    DynInstr *getDynInstr();
    llvm::Value *getStaticValue();
    bool isConstant();    
  };
}

#endif

