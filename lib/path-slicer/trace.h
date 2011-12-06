#ifndef __TERN_PATH_SLICER_TRACE_H
#define __TERN_PATH_SLICER_TRACE_H

#include <list>

#include "stat.h"
#include "dyn-instr.h"

namespace tern {
  class Trace {
  private:
    Stat *stat;
    std::list<DynInstr *> origTrace;
    std::list<DynInstr *> slice;
    std::list<DynInstr *> targets;

  protected:

  public:
    Trace();
    ~Trace();
    
  };
}

#endif
