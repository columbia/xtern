#ifndef __TERN_PATH_SLICER_TRACE_UTIL_FOR_XTERN_H
#define __TERN_PATH_SLICER_TRACE_UTIL_FOR_XTERN_H

#include "trace-util.h"

namespace tern {
  /* Class to record and load trace. Work with xtern. */
  class XTernTraceUtil: public TraceUtil {
  private:

  protected:

  public:
    XTernTraceUtil();
    ~XTernTraceUtil();
    virtual void load(const char *tracePath, DynInstrVector *trace);
    virtual void store(const char *tracePath);
    virtual void record(DynInstrVector *trace, void *instr, void *state, void *f);
    virtual void preProcess(DynInstrVector *trace);
    virtual void postProcess(DynInstrVector *trace);
  };
}

#endif

