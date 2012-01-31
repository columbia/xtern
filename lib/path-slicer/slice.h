#ifndef __TERN_PATH_SLICER_SLICE_SET_H
#define __TERN_PATH_SLICER_SLICE_SET_H

#include <list>

#include "dyn-instrs.h"

namespace tern {
  class Slice {
  private:
    std::list<DynInstr *> instsList; /* The list of taken dynamic instructions. */
    std::set<DynInstr *> instsSet; /* The set of the taken dynamic instructions, for fast lookup. */

  protected:

  public:
    Slice();
    ~Slice();
    void add(DynInstr *dynInstr, unsigned char reason);
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

