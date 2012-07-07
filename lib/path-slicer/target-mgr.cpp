#include "macros.h"
#include "target-mgr.h"
using namespace tern;

using namespace llvm;

using namespace klee;

TargetMgr::TargetMgr() {
  targets.clear();
  targetMasks.clear();
}

TargetMgr::~TargetMgr() {

}

void TargetMgr::markTarget(void *pathId, DynInstr *dynInstr, uchar reason, klee::Checker :: ResultType mask) {
  if (targets.count(pathId) == 0)
    targets[pathId] = new DenseSet<DynInstr *>;
  dynInstr->setTaken(reason);
  targets[pathId]->insert(dynInstr);
  setTargetMask(dynInstr,  mask);
}

size_t TargetMgr::getLargestTargetIdx(void *pathId) {
  assert(hasTarget(pathId));
  size_t max = 0;
  DenseSet<DynInstr *>::iterator itr(targets[pathId]->begin());
  for (; itr != targets[pathId]->end(); ++itr) {
    DynInstr *dynInstr = *itr;
    if (max < dynInstr->getIndex())
      max = dynInstr->getIndex();
  }
  return max;
}

bool TargetMgr::hasTarget(void *pathId) {
  return targets.count(pathId) > 0 && targets[pathId]->size() > 0;
}

void TargetMgr::copyTargets(void *newPathId,void *curPathId) {
  if (!hasTarget(curPathId))
    return;
  if (targets.count(newPathId) == 0)
    targets[newPathId] = new DenseSet<DynInstr *>;
  targets[newPathId]->insert(targets[curPathId]->begin(), targets[curPathId]->end());
}

void TargetMgr::clearTargets(void *pathId) {
  //fprintf(stderr, "TargetMgr::clearTargets start\n");
  if (!hasTarget(pathId))
    return;
  DenseSet<DynInstr *> *targetSet = targets[pathId];
  targetSet->clear();
  targets.erase(pathId);
  delete targetSet;
  assert(!hasTarget(pathId));
  //fprintf(stderr, "TargetMgr::clearTargets end\n");
}

void TargetMgr::setTargetMask(DynInstr *dynInstr, klee::Checker::ResultType mask) {
  targetMasks[dynInstr] = mask;
}

klee::Checker::ResultType TargetMgr::getTargetMask(DynInstr *dynInstr) {
  assert(DM_IN(dynInstr, targetMasks));
  return targetMasks[dynInstr];
}

