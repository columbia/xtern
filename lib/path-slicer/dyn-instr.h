#ifndef __TERN_PATH_SLICER_DYN_INSTR_H
#define __TERN_PATH_SLICER_DYN_INSTR_H

#include "dyn-oprd.h"
#include "instr-region.h"

namespace tern {
  class InstrRegion;
  /* We must include as few as fields in this class in order to save memory. 
    The main idea to get memory efficiency is to keep as many as information in its region.
    Only keep fields that MUST belong to all dynamic instructions in this class. */
  class DynInstr {
  private:
    /* A pointer to the region that this instr belongs to, the region contains as many as shared info
      of instructions within this region, such as thread id and vector clock. */
    InstrRegion *region;

    /* All dynamic operands of this instruction, including called function and function arguments.
      It contains both dest oprd and src oprds. */
    std::vector<DynOprd * > *oprds;

    /* The total order of execution index of this dynamic instr.
        Question: does xtern still have this feature? */
    size_t index;

    /* In normal and max slicing mode, this is the normal and max sliced calling context, respectively. */
    std::vector<int> *callingCtx;

    /* This is only valid in simplified slicing mode. 
      TODO: MUST THINK OF A NICE WAY TO ELIMINATE THIS MEMORY TOO. */
    std::vector<int> *simCallingCtx;

  protected:

  public:
    DynInstr();
    ~DynInstr();
    int getTid();
    size_t getIndex();
    void setCallingCtx(std::vector<int> &ctx);
    std::vector<int> *getCallingCtx();
    void setSimCallingCtx(std::vector<int> &ctx);
    std::vector<int> *getSimCallingCtx();
    int getOrigInstrId();
    int getMxInstrId();
    std::set<int> *getSimInstrId();
    void setTaken(bool isTaken, std::string reason = NOT_TAKEN_INSTR);
    bool isTaken();
    int getPhiIncomeIdx();
    // TBD.
  };

  /* Iterator of a dynamic instruction. */
  typedef std::list<DynInstr *>::iterator DynInstrItr;
}

#endif

