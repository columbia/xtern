#ifndef __TERN_PATH_SLICER_INTER_SLICER_H
#define __TERN_PATH_SLICER_INTER_SLICER_H

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "dyn-instr.h"

namespace tern {
  struct InterSlicer {
  private:


  protected:
        

  public:
    InterSlicer();
    ~InterSlicer();
    void detectInputDepRaces(InstrRegions *instrRegions);
  };
}

#endif

