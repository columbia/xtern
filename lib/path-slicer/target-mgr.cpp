#include "macros.h"
#include "target-mgr.h"
using namespace tern;

using namespace llvm;

TargetMgr::TargetMgr() {
  targets.clear();
  largestIdx = (size_t)-1;
}

TargetMgr::~TargetMgr() {

}

void TargetMgr::markTarget(DynInstr *dynInstr, uchar reason) {
  dynInstr->setTaken(reason);
  targets.insert(dynInstr);
  if (largestIdx == (size_t)-1)
    largestIdx = dynInstr->getIndex();
  else if (dynInstr->getIndex() > largestIdx)
    largestIdx = dynInstr->getIndex();
}

void TargetMgr::markTargetOfCall(DynCallInstr *dynCallInstr) {
  dynCallInstr->setContainTarget(true);
}

size_t TargetMgr::getLargestTargetIdx() {
  return largestIdx;
}

bool TargetMgr::hasTarget() {
  return targets.size() > 0;
}

