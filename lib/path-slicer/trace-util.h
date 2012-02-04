
#ifndef __TERN_PATH_SLICER_TRACE_UTIL_H
#define __TERN_PATH_SLICER_TRACE_UTIL_H

#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"

#include "dyn-instrs.h"
#include "type-defs.h"
#include "stat.h"
#include "callstack-mgr.h"

#define TRACE_MAX_LEN 1024*1024*1024  /* 1G of binary trace. */

namespace tern {
  /* Generic and abstract class to record and load trace. */
  class TraceUtil {
  private:

  protected:
    CallStackMgr *ctxMgr;


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
    virtual void record(DynInstrVector *trace, void *instr, void *state, void *f) = 0;

    /* After a complete trace is recorded, we do some pre-processing work such as calculating
    incoming index for PHI instruction, and setting up callstack. */
    virtual void preProcess(DynInstrVector *trace) = 0;

    /* After slicing, free the trace. */
    virtual void postProcess(DynInstrVector *trace) = 0;

    void initCallStackMgr(tern::CallStackMgr *ctxMgr);
  };
}

#endif


