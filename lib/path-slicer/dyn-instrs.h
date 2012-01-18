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
    InstrRegion *region;

    /* The total order of execution index of this dynamic instr.
        Question: does xtern still have this feature? */
    size_t index;

    /* In normal and max slicing mode, this is the normal and max sliced calling context, respectively. */
    std::vector<int> *callingCtx;

    /* This is only valid in simplified slicing mode. 
      For each dynamic instruction, there should be only one callstack in simplified bc, right?
      TODO: MUST THINK OF A NICE WAY TO ELIMINATE THIS MEMORY TOO. */
    std::vector<int> *simCallingCtx;

  public:
    DynInstr();
    ~DynInstr();

    /* TBD */
    int getTid();

    /* TBD */
    size_t getIndex();

    /* TBD */
    void setCallingCtx(std::vector<int> &ctx);

    /* TBD */
    std::vector<int> *getCallingCtx();

    /* TBD */
    void setSimCallingCtx(std::vector<int> &ctx);

    /* TBD */
    std::vector<int> *getSimCallingCtx();

    /* TBD */
    int getOrigInstrId();

    /* TBD */
    int getMxInstrId();

    /* TBD */
    std::set<int> *getSimInstrId();

    /* TBD */
    void setTaken(bool isTaken, std::string reason = NOT_TAKEN_INSTR);

    /* TBD */
    bool isTaken();

    /* Whether an instruction is a slicing target already, currently it is the same as taken. */
    bool isTarget();

    /* TBD */
    llvm::Instruction *getOrigInstr();
  };

  class DynPHIInstr: public DynInstr {
  private:
    unsigned incomingIndex;

  protected:
    
  public:
    DynPHIInstr();
    ~DynPHIInstr();
    void setIncomingIndex(unsigned index);
    unsigned getIncomingIndex();
    
  };

  class DynBrInstr: public DynInstr {
  private:

  protected:

  public:
    DynBrInstr();
    ~DynBrInstr();
    
  };

  class DynRetInstr: public DynInstr {
  private:
    /* The dynamic call instruction corresponds to this return instruction. */
    DynInstr *dynCallInstr;

  protected:

  public:
    DynRetInstr();
    ~DynRetInstr();
    void setDynCallInstr(DynInstr *dynInstr);
    DynInstr *getDynCallInstr();
    
  };

  class DynCallInstr: public DynInstr {
  private:

  protected:
    /* This called function is the recorded one during execution, so it can solve the inambiguity
    in function pointers. Need to add constraint assertion to fix this called one. */
    llvm::Function *calledFunc;

  public:
    DynCallInstr();
    ~DynCallInstr();
    void setCalledFunc(llvm::Function *f);
    llvm::Function *getCalledFunc();
    bool isInternalCall();
    
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
    long memAddr;
    bool isAddrSymbolic;

  protected:

  public:
    DynMemInstr();
    ~DynMemInstr();
    void setMemAddr(long memAddr);
    long getMemAddr();
    void setMemAddrSymbolic(bool sym);
    bool isMemAddrSymbolic();
  };
}

#endif


