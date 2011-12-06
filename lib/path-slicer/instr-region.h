#ifndef __TERN_PATH_SLICER_INSTR_REGION_H
#define __TERN_PATH_SLICER_INSTR_REGION_H

#include "dyn-instr.h"

/*  A Instruction Region is a continuous stream of executed instructions within one thread.
  It starts from one synch operation and ends with another one (not including the this one).
  
*/

namespace tern {
  class InstrRegion {
  private:
    long startSyncId;
    long endSyncId;
    int tid;
    list<DynInstr *> instrs;

  protected:

  public:
    InstrRegion();
    ~InstrRegion();
    
    
  };
}

#endif

