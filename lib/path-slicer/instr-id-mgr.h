#ifndef __TERN_PATH_SLICER_INSTR_ID_MGR_H
#define __TERN_PATH_SLICER_INSTR_ID_MGR_H

#include "slicer/clone-info-manager.h"
#include "slicer/trace.h"

#include "common/IDAssigner.h"
#include "common/util.h"
#include "slicer/max-slicing.h"
#include "slicer/region-manager.h"
#include "slicer/landmark-trace-record.h"

#include "llvm/Instruction.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "type-defs.h"
#include "dyn-instrs.h"
#include "macros.h"
#include "stat.h"

namespace tern {
  class DynOprd;
  class DynInstr;
  class InstrIdMgr {
  private:
    Stat *stat;
    
    /* Original LLVM Module. */
    llvm::Module *origModule;    

    /* Max Sliced Module. */
    llvm::Module *mxModule;       

    /* Simplified Module. */
    llvm::Module *simModule;      

    vector<LandmarkTraceRecord *> binLTR;

    /* We need both max sliced and simplified map since in some slicing mode we query max sliced bc
      for alias first, and then query the simplified bc. */
      
    /* Map from a dynamic instr to its instr id in max sliced module. */
    HMAP<LongLongPair, int> mxMap;

    /* Map from a dynamic max sliced instr id to it original instr id. */
    HMAP<int, int> revMxMap;                 

    /* Map from a dynamic instr to its instr id in simplified module. */
    HMAP<LongLongPair, std::set<int> *> simMap;   

    /* Map from a dynamic simplified instr id to it original instr id. */
    HMAP<int, int> revSimMap;               
    
    /* Map from a dynamic simplified instr id to it original instr. */
    HMAP<LongLongPair, std::set<Instruction *> *> simInstrMap; 

  protected:
    /* Generate a long long pair key for a dynamic instr. */
    LongLongPair genKey(DynInstr *dynInstr);

    /* Internal debugging utilities. */
    bool isOrigBcInstr(const llvm::Instruction *instr);
    bool isMxBcInstr(const llvm::Instruction *instr);
    bool isSimBcInstr(const llvm::Instruction *instr);

  public:
    InstrIdMgr();
    ~InstrIdMgr();
    void initStat(Stat *stat);
    void initModules(llvm::Module *origModule, llvm::Module *mxModule, llvm::Module *simModule,
      const char *lmTracePath);
    /* Load the landmark trace (not full trace) from disk, this is important to setup the instr id mapping. */
    void loadBinLandmarkTrace(const char *fullPath);

    /* Given a dynamic instr, return its instr id in max sliced module. */
    int getMxInstrId(const DynInstr *dynInstr);

    /* Given a dynamic instr, return its instr in max sliced module. */
    llvm::Instruction *getMxInstr(const DynInstr *dynInstr);

    /* Given a dynamic instr, return its set of instr id in simplified module. */
    std::set<int *> *getSimInstrId(const DynInstr *dynInstr);

    /* Given a dynamic instr, return its set of instr in simplified module. */
    set<llvm::Instruction *> *getSimInstr(const DynInstr *dynInstr);
  };
}

#endif

