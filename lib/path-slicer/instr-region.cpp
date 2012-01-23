#include "instr-region.h"
using namespace tern;

InstrRegion::InstrRegion() {

}

InstrRegion::~InstrRegion() {

}

void InstrRegion::setTaken(DynInstr *dynInstr, bool isTaken, const char *reason) {
  if (isTaken)
    takenInstrs[dynInstr] = reason;
}

bool InstrRegion::isTaken(DynInstr *dynInstr) {
  return DM_IN(dynInstr, takenInstrs);
}

