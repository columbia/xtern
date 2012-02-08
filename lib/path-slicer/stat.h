#ifndef __TERN_PATH_SLICER_STAT_H
#define __TERN_PATH_SLICER_STAT_H

#include <string>
#include <ext/hash_map>

#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

#include "macros.h"

namespace tern {
  class DynInstr;
  class InstrIdMgr;
  class CallStackMgr;
  class Stat {
  private:
    InstrIdMgr *idMgr;
    CallStackMgr *ctxMgr;
    HMAP<long, llvm::raw_string_ostream * > buf;

  protected:

  public:
    struct timeval interSlicingSt;
    struct timeval interSlicingEnd;
    double interSlicingTime;

    struct timeval intraSlicingSt;
    struct timeval intraSlicingEnd;
    double intraSlicingTime;

    Stat();
    ~Stat();
    void init(InstrIdMgr *idMgr, CallStackMgr *ctxMgr);
    void printStat(const char *tag = NULL);
    const char *printInstr(const llvm::Instruction *instr);    
    const char *printValue(const llvm::Value *v);
    void printDynInstr(DynInstr *dynInstr, const char *tag = NULL);
  };
}

#endif

