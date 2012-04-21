#ifndef __TERN_PATH_SLICER_RACY_EDGE_H
#define __TERN_PATH_SLICER_RACY_EDGE_H

#include "dyn-instrs.h"

namespace tern {
  /* 
    The execution order constraints we are going to enforce at replay in order to resolve races 
    deterministically.
    TODO: the instruction region is separated as the total order of synch operations,
    but if we enforce a racy edge, how are we going to "quantify" the logical clocks for new splitted
    regions, given the execution order constraints are partial order.
  */
  class RacyEdge {
  private:
    /* An execution order constraint is enforced as the "prev" must always execute before "post". */
    DynInstr *prev;
    DynInstr *post;

  protected:

  public:
    RacyEdge(DynInstr *prev, DynInstr *post);
    ~RacyEdge();    
  };
}

#endif



