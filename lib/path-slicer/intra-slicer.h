#ifndef __TERN_PATH_SLICER_INTRA_SLICER_H
#define __TERN_PATH_SLICER_INTRA_SLICER_H

#include <list>

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include "stat.h"
#include "dyn-instr.h"
#include "slice.h"
#include "live-set.h"
#include "alias-mgr.h"
#include "cfg-mgr.h"

namespace tern {
  /* Note:
    Current slicing algorithm which suits for PEREGRINE ignores "values", which would be required by 
    the directed symbolic execution project.
  */
  struct IntraSlicer {
  private:
    Stat *stat;
    LiveSet live;
    Slice *slice;
    AliasMgr *aliasMgr;
    CfgMgr *cfgMgr;
    

  protected:
    void take(LiveSet &live, Slice &slice, DynInstr *dynInstr, const char *reason);
    bool mayWrite(DynInstr *dynInstr, LiveSet &live);
    bool mayWriteFunc(DynInstr *dynRetInstr, llvm::Function *func, LiveSet &live);
    bool writtenBetween(DynInstr *dynInstr, DynInstr *dynHeadInstr, LiveSet &live);
    llvm::Instruction *getStaticPostDominator(DynInstr *dynInstr);
    bool postDominate(DynInstr *dynPostInstr, DynInstr *dynPrevInstr);
    bool postDominate(int postIid, int curIid);
    DynInstrItr current();

  public:
    IntraSlicer();
    ~IntraSlicer();
    void detectInputDepRaces(DynInstrItr startInstr);
  };
}

#endif

