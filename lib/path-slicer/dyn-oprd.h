#ifndef __TERN_PATH_SLICER_DYN_OPRD_H
#define __TERN_PATH_SLICER_DYN_OPRD_H

#include "dyn-instr.h"

namespace tern {
  /* We must include as few as fields in this class in order to save memory. */
  class DynOprd {
  private:
    /* Points back to the dynamic instruction that owns this dynamic operand. */
    DynInstr *dynInstr;

  protected:

  public:
    DynOprd();
    ~DynOprd();
    
  };
}

#endif

