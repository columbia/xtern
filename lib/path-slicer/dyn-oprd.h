#ifndef __TERN_PATH_SLICER_DYN_OPRD_H
#define __TERN_PATH_SLICER_DYN_OPRD_H

#include "dyn-instr.h"

#include "llvm/Instruction.h"

namespace tern {
  class DynInstr;
  /* We must include as few as fields in this class in order to save memory. */
  class DynOprd {
  private:
    /* Points back to the dynamic instruction that owns this dynamic operand. */
    DynInstr *dynInstr;
    int oprdIndex; /* Start from -1, which is the dest oprd. */

    bool isAddrSymbolic;
    bool isConstant;
    uint64_t memAddr;

  protected:

  public:
    DynOprd();
    ~DynOprd();
    bool isVariable();
    int getIndex();
    bool isDestOprd();
    llvm::Value *getStaticValue();
    
  };
}

#endif

