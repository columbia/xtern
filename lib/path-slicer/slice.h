#ifndef __TERN_PATH_SLICER_SLICE_SET_H
#define __TERN_PATH_SLICER_SLICE_SET_H

#include <list>

#include "llvm/ADT/DenseSet.h"

#include "dyn-instrs.h"
#include "stat.h"

namespace tern {
  class Slice {
  private:
    Stat *stat;
    std::list<DynInstr *> instsList; /* The list of taken dynamic instructions. */
    llvm::DenseSet<DynInstr *> instsSet; /* The set of the taken dynamic instructions, for fast lookup. */

  protected:

  public:
    Slice();
    ~Slice();
    void init(Stat *stat);
    void add(DynInstr *dynInstr, uchar reason);
    void dump();
    size_t size();
    void clear();
    bool in(DynInstr *dynInstr);
    DynInstrItr begin();
    DynInstrItr end();
    DynInstr *getHead();
  };
}

#endif

