#ifndef __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H
#define __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H

#include "trace-util.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"

namespace tern {
  /* Class to record and load trace. Work with KLEE. */
  class KleeTraceUtil: public TraceUtil {
  private:

  protected:
    void record(klee::KInstruction *instr, klee::ExecutionState *state);

  public:
    KleeTraceUtil();
    ~KleeTraceUtil();
    virtual void load(const char *tracePath, DynInstrVector *trace);
    virtual void store(const char *tracePath);
    virtual void record(void *instr, void *state);
  };
}

#endif

