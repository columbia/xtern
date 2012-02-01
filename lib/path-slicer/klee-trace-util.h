#ifndef __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H
#define __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H

#include "common/IDAssigner.h"

#include "klee/Expr.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/KModule.h"

#include "trace-util.h"

namespace tern {
  /* Class to record and load trace. Work with KLEE. */
  class KleeTraceUtil: public TraceUtil {
  private:
    klee::KModule *kmodule;
    //DynInstrVector *trace;
    llvm::IDAssigner *idAssigner;

  protected:
    /* Record routines. */
    void record(DynInstrVector *trace, klee::KInstruction *kInstr,
      klee::ExecutionState *state, llvm::Function *f);
    void recordPHI(DynInstrVector *trace, klee::KInstruction *kInstr,
      klee::ExecutionState *state);
    void recordBr(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ExecutionState *state);
    void recordRet(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ExecutionState *state);
    void recordCall(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ExecutionState *state, llvm::Function *f);
    void recordNonMem(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ExecutionState *state);
    void recordLoad(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ExecutionState *state);
    void recordStore(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ExecutionState *state);    
    /* Borrowed from KLEE Executor.cpp */
    const klee::Cell& eval(klee::KInstruction *ki, unsigned index, 
      klee::ExecutionState &state) const;

  public:
    KleeTraceUtil();
    ~KleeTraceUtil();

    /* This function must be called before uclibc is linked in. */
    void initKModule(klee::KModule *kmodule);
    virtual void load(const char *tracePath, DynInstrVector *trace);
    virtual void store(const char *tracePath);
    
    /* This is the key function inserted to KLEE interpreter to record each instruction
    (at the place before they are executed). */
    virtual void record(DynInstrVector *trace, void *instr, void *state, void *f);
    virtual void preProcess(DynInstrVector *trace);
  };
}

#endif

