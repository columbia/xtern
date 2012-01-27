#ifndef __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H
#define __TERN_PATH_SLICER_TRACE_UTIL_FOR_KLEE_H

#include "klee/Expr.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/KModule.h"

#include "trace-util.h"

namespace tern {
  /*class InstrRecord {
  private:

  protected:
    int tid;
    int instrId;

  public:
    InstrRecord();
    ~InstrRecord();
    void setTid(int tid);
    int getTid();
    void setInstrId(int instrId);
    int getInstrId();
  };
  class MemInstrRecord: public InstrRecord {
  private:

  protected:
    long conAddr;
    klee::ref<klee::Expr> symAddr;

  public:
    MemInstrRecord();
    ~MemInstrRecord();
    void setSymAddr(klee::ref<klee::Expr> addr);
    klee::ref<klee::Expr> getSymAddr();
    void setConAddr(long addr);
    long getConAddr();
    bool isAddrConstant();
  };*/
  /* Class to record and load trace. Work with KLEE. */
  class KleeTraceUtil: public TraceUtil {
  private:
    klee::KModule *kmodule;
    DynInstrVector *trace;
    /*int fd;
    unsigned long offset;
    char *buf;*/

  protected:
    /* Record routines. */
    void record(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordPHI(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordBr(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordRet(klee::KInstruction *kInstr, klee::ExecutionState *state);
    void recordCall(klee::KInstruction *kInstr, klee::ExecutionState *state);
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
    void initKModule(klee::KModule *kmodule);
    void initTrace(DynInstrVector *trace);
    virtual void load(const char *tracePath, DynInstrVector *trace);
    virtual void store(const char *tracePath);
    
    /* This is the key function inserted to KLEE interpreter to record each instruction
    (at the place before they are executed). */
    virtual void record(void *instr, void *state);
  };
}

#define KLEE_RECORD_SIZE (sizeof(MemInstrRecord))

#endif

