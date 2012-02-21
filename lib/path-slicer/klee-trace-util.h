#ifndef __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H
#define __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H

#include "common/IDAssigner.h"

#include "klee/Expr.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/KModule.h"

#include "stat.h"
#include "instr-id-mgr.h"
#include "trace-util.h"

namespace tern {
  /* Class to record and load trace. Work with KLEE. */
  class KleeTraceUtil: public TraceUtil {
  private:
    klee::KModule *kmodule;
    InstrIdMgr *idMgr;
    Stat *stat;

  protected:
    /* Record routines. */
    void record(DynInstrVector *trace, klee::KInstruction *kInstr,
      klee::ThreadState *state, llvm::Function *f);
    void recordPHI(DynInstrVector *trace, klee::KInstruction *kInstr,
      klee::ThreadState *state);
    void recordBr(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ThreadState *state);
    void recordRet(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ThreadState *state);
    void recordCall(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ThreadState *state, llvm::Function *f);
    void recordNonMem(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ThreadState *state);
    void recordLoad(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ThreadState *state);
    void recordStore(DynInstrVector *trace, klee::KInstruction *kInstr, 
      klee::ThreadState *state);    
    /* Borrowed from KLEE Executor.cpp */
    const klee::Cell& eval(klee::KInstruction *ki, unsigned index, 
      klee::ThreadState &state) const;

  public:
    KleeTraceUtil();
    ~KleeTraceUtil();

    void init(InstrIdMgr *idMgr, Stat *stat);

    /* This function is called after uclibc is linked in. */
    void initKModule(klee::KModule *kmodule);

    virtual void load(const char *tracePath, DynInstrVector *trace);
    virtual void store(void *pathId, DynInstrVector *trace);
    
    /* This is the key function inserted to KLEE interpreter to record each instruction
    (at the place before they are executed). */
    virtual void record(DynInstrVector *trace, void *instr, void *state, void *f);
    virtual void preProcess(DynInstrVector *trace);
    virtual void postProcess(DynInstrVector *trace);
  };
}

#endif

