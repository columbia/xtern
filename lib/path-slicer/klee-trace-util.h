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
    DynInstrVector *trace;
    llvm::IDAssigner *idAssigner;

  protected:
    /* Record routines. */
    void record(klee::KInstruction *kInstr, klee::ExecutionState *state, llvm::Function *f);
    void recordPHI(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordBr(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordRet(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordCall(klee::KInstruction *kInstr, klee::ExecutionState *state, llvm::Function *f);
    void recordNonMem(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordLoad(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordStore(klee::KInstruction *kInstr, klee::ExecutionState *state);

    /* After a complete trace is recorded, we do some processing work such as calculating
    incoming index for PHI instruction, and setting up callstack. */
    void processTrace();
    
    /* Borrowed from KLEE Executor.cpp */
    const klee::Cell& eval(klee::KInstruction *ki, unsigned index, 
      klee::ExecutionState &state) const;

  public:
    KleeTraceUtil();
    ~KleeTraceUtil();

    /* This function must be called before uclibc is linked in. */
    void initKModule(klee::KModule *kmodule);
    void initTrace(DynInstrVector *trace);
    virtual void load(const char *tracePath, DynInstrVector *trace);
    virtual void store(const char *tracePath);
    
    /* This is the key function inserted to KLEE interpreter to record each instruction
    (at the place before they are executed). */
    virtual void record(void *instr, void *state, void *f);
  };
}

#endif

