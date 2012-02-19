#ifndef __TERN_PATH_SLICER_DYN_INSTRS_H
#define __TERN_PATH_SLICER_DYN_INSTRS_H

#include <vector>
#include <list>
namespace tern {
  class DynInstr;
  /* Composed types of a dynamic instruction. */
  typedef std::vector<DynInstr *> DynInstrVector;
  typedef std::list<DynInstr *> DynInstrList;
  typedef DynInstrList::iterator DynInstrItr;
}

#include "klee/Expr.h"

#include "dyn-oprd.h"
#include "instr-region.h"

namespace tern {
  class InstrRegion;
  /* We must include as few as fields in this class in order to save memory. 
    The main idea to get memory efficiency is to keep as many as information in its region.
    Only keep fields that MUST belong to all dynamic instructions in this class. */
  class DynInstr {
  private:

  protected:
    /* A pointer to the region that this instr belongs to, the region contains as many as shared info
      of instructions within this region, such as thread id and vector clock. */
    //InstrRegion *region;
    // TBD: this should be here when I start to implement inter-thread phase.

    /* The total order of execution index of this dynamic instr.
        Question: does xtern still have this feature? */
    size_t index;

    /* In normal and max slicing mode, this is the normal and max sliced calling context, respectively. */
    CallCtx *callingCtx;

    /* This is only valid in simplified slicing mode. 
      For each dynamic instruction, there should be only one callstack in simplified bc, right?
      TODO: MUST THINK OF A NICE WAY TO ELIMINATE THIS MEMORY TOO. */
    //CallCtx *simCallingCtx;

    /* The static instruction id from orig bc module. */
    int instrId;

    /* The xtern self-maintanied thread id. */
    uchar tid;

    /* Identify a dynamic instruction is taken or not; 0 is not taken; and !0 means taken reaons. */
    uchar takenFlag;

  public:
    DynInstr();
    ~DynInstr();

    /* TBD */
    void setTid(uchar tid);

    /* TBD */
    uchar getTid();

    /* TBD */
    void setIndex(size_t index);

    /* TBD */
    size_t getIndex();

    /* TBD */
    void setCallingCtx(std::vector<int> *ctx);

    /* TBD */
    CallCtx *getCallingCtx();

    /* TBD */
    //void setSimCallingCtx(std::vector<int> *ctx);

    /* TBD */
    //CallCtx *getSimCallingCtx();

    /* TBD */
    void setOrigInstrId(int instrId);

    /* TBD */
    int getOrigInstrId();

    /* TBD */
    void setTaken(uchar takenFlag);

    /* TBD */
    bool isTaken();

    /* TBD */
    uchar getTakenFlag();

    /* TBD */
    const char *takenReason();

    /* Whether an instruction is a inter-thread phase slicing target. */
    bool isInterThreadTarget();

    /* Whether an instruction is marked as IMPORTANT returned from the checker. */
    bool isCheckerTarget();

    bool isTarget();
  };

  class DynPHIInstr: public DynInstr {
  private:
    uchar incomingIndex;

  protected:
    
  public:
    DynPHIInstr();
    ~DynPHIInstr();
    void setIncomingIndex(uchar index);
    uchar getIncomingIndex();
  };

  class DynBrInstr: public DynInstr {
  private:
    /* These flag only makes sense when the branch is not uni-conditional. */
    klee::ref<klee::Expr> condition;
    llvm::BasicBlock *successor;

  protected:

  public:
    DynBrInstr();
    ~DynBrInstr();

    /* These functions only make sense when the branch is not uni-conditional. */
    bool isSymbolicBr();
    void setBrCondition(klee::ref<klee::Expr> condition);
    klee::ref<klee::Expr> getBrCondition();
    void setSuccessorBB(llvm::BasicBlock *successor);
    llvm::BasicBlock *getSuccessorBB();
  };

  class DynCallInstr: public DynInstr {
  private:

  protected:
    /* This called function is the recorded one during execution, so it can solve the inambiguity
    in function pointers. Need to add constraint assertion to fix this called one. */
    llvm::Function *calledFunc;

    /* Mark whether a function call contains any target instruction (e.g., inter-thread target or checker target).
    The bool type can be replaced as a set of dyninstr * which are targets.
    FIXME: when we implement the inter-thread phase, the call stack is built before inter-thread phase
    starts to work, so we need to repick those targets. */
    bool containTarget;   

  public:
    DynCallInstr();
    ~DynCallInstr();
    void setCalledFunc(llvm::Function *f);
    llvm::Function *getCalledFunc();
    void setContainTarget(bool containTarget);
    bool getContainTarget();
  };

  class DynRetInstr: public DynInstr {
  private:
    /* The dynamic call instruction corresponds to this return instruction. */
    DynCallInstr *dynCallInstr;

  protected:

  public:
    DynRetInstr();
    ~DynRetInstr();
    void setDynCallInstr(DynCallInstr *dynInstr);
    DynCallInstr *getDynCallInstr();
    
  };

  class DynSpawnThreadInstr: public DynCallInstr {
  private:
    int childTid;

  protected:
    

  public:
    DynSpawnThreadInstr();
    ~DynSpawnThreadInstr();
    void setChildTid(int childTid);
    int getChildTid();
  };

  class DynMemInstr: public DynInstr {
  private:
    /* Loaded or stored memory address from the pointer operand. */
    klee::ref<klee::Expr> addr;

  protected:

  public:
    DynMemInstr();
    ~DynMemInstr();
    void setAddr(klee::ref<klee::Expr> symAddr);
    klee::ref<klee::Expr> getAddr();
    bool isAddrSymbolic();
  };
}

#endif


