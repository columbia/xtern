#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "inter-slicer.h"
#include "path-slicer.h"
using namespace tern;

InterSlicer::InterSlicer() {
}

InterSlicer::~InterSlicer() {

}

void InterSlicer::detectInputDepRaces(InstrRegions *instrRegions) {
  // TBD.
}

void InterSlicer::calStat() {
  // TBD: print dynamic intrunctions taken as slicing targets.
}

