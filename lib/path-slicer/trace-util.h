
#ifndef __TERN_PATH_SLICER_TRACE_UTIL_H
#define __TERN_PATH_SLICER_TRACE_UTIL_H

#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"

#include "dyn-instrs.h"
#include "type-defs.h"
#include "stat.h"

namespace tern {
  /* Generic and abstract class to record and load trace. */
  class TraceUtil {
  private:

  protected:
    DynInstrVector recordedInstrs;

  public:
    TraceUtil();
    ~TraceUtil();
    
    /* Load all recorded instructions from the trace path to memory; loaded results are stored in
    to the trace vector. */
    virtual void load(const char *tracePath, DynInstrVector *trace) = 0;

    /* Store all recorded instructions to a specified trace path. */
    virtual void store(const char *tracePath) = 0;

    /* Record one dynamic instruction. The passed in pointer can be any type depending on
    recording targets (KLEE or xtern). */
    virtual void record(void *instr, void *state) = 0;
  };
}

#endif


