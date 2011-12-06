#ifndef __TERN_INTER_SLICER_H
#define __TERN_INTER_SLICER_H

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "dyn-instr.h"
#include "trace.h"

namespace tern {
  struct InterSlicer {
    
  public:
    InterSlicer();
    ~InterSlicer();
    void detectInputDepRaces(Trace &trace);
  };
}

#endif

