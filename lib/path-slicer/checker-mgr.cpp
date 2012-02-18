#include "macros.h"
#include "checker-mgr.h"
using namespace tern;

using namespace llvm;

CheckerMgr::CheckerMgr() {
  checkerTargets.clear();
  largestIdx = (size_t)-1;
}

CheckerMgr::~CheckerMgr() {

}

void CheckerMgr::markCheckerTarget(DynInstr *dynInstr) {
  dynInstr->setTaken(CHECKER_TARET);
  checkerTargets.insert(dynInstr);
  if (largestIdx == (size_t)-1)
    largestIdx = dynInstr->getIndex();
  else if (dynInstr->getIndex() > largestIdx)
    largestIdx = dynInstr->getIndex();
}

void CheckerMgr::markCheckerTargetOfCall(DynCallInstr *dynCallInstr) {
  dynCallInstr->setContainTarget(true);
}

size_t CheckerMgr::getLargestCheckerIdx() {
  return largestIdx;
}

bool CheckerMgr::hasTarget() {
  return checkerTargets.size() > 0;
}

