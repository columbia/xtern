#ifndef __TERN_PATH_SLICER_INSTR_REGION_H
#define __TERN_PATH_SLICER_INSTR_REGION_H

#include <set>
#include <list>

#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"

#include "type-defs.h"
#include "macros.h"
#include "dyn-instrs.h"
#include "instr-id-mgr.h"
#include "rw-set.h"

/*  A Instruction Region is a continuous stream of executed instructions within one thread.
  It starts from one synch operation and ends with another one (not including the this one).
  
*/

namespace tern {
  class InstrRegion {
  private:
    /* A complete trace. */
    DynInstrVector *trace;

    /* The starting point of instruction index in the trace (inclusive). */
    size_t startIndex;

    /* The end point of instruction index in the trace (exclusive). */
    size_t endIndex;

    /* Thread id. When going over from the start to end of the trace, only pick the dynamic
    instructions generated from current thread id. */
    int tid;
    
    /* Inclusive total order of the starting sync event. */
    int startSyncId;

    /* Exclusive (if it is not thread/process exits) total order of the starting sync event. */    
    int endSyncId;

    /* All the taken dynamic instructions in slicing.
        TODO: Probably we can make the string to be only a char pointer, which could save 
        much memory. */
    llvm::DenseMap<DynInstr *, const char *> takenInstrs;
    
    /* We need to have per instr region cache for read write set, this is because the read write
    set of each instr region can be different. */
    RWSet readSet;
    RWSet writeSet;

  protected:

  public:
    InstrRegion();
    ~InstrRegion();
    void setTid(int tid);
    int getTid();
    void setVecClock(int start, int end);
    void getVecClock(int &start, int &end);
    void addDynInstr(DynInstr *dynInstr);
    static bool isConcurrent(InstrRegion *region1, InstrRegion *region2);
    bool happensBefore(InstrRegion *region);
    void setTaken(DynInstr *dynInstr, bool isTaken,
      const char *reason = NOT_TAKEN_INSTR);
    bool isTaken(DynInstr *dynInstr);
    void setBrIncomeIdx(DynInstr *dynInstr, int index);
    int getBrIncomeIdx(DynInstr *dynInstr);
    void setChildTid(DynInstr *dynInstr, int childTid);
    int getChildTid(DynInstr *dynInstr);
    int getOrigInstrId(DynInstr *dynInstr);
    int getMxInstrId(DynInstr *dynInstr);
    std::set<int> *getSimInstrId(DynInstr *dynInstr);
    llvm::Instruction *getOrigInstr(DynInstr *dynInstr);
    llvm::Instruction *getMxInstr(DynInstr *dynInstr);
    std::set<llvm::Instruction * > *getSimInstr(DynInstr *dynInstr);
  };

  /* Regions. */
  typedef std::list<InstrRegion *> InstrRegions;
}

#endif

