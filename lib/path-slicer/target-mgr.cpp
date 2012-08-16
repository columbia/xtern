#include <map>
using namespace std;

#include "macros.h"
#include "target-mgr.h"
using namespace tern;

using namespace llvm;

using namespace klee;

TargetMgr::TargetMgr() {
  stat = NULL;
  targets.clear();
  targetMasks.clear();
}

TargetMgr::~TargetMgr() {

}

void TargetMgr::init(Stat *stat) {
  this->stat = stat;
}

void TargetMgr::markTarget(void *pathId, DynInstr *dynInstr, uchar reason, klee::Checker :: ResultType mask) {
  if (targets.count(pathId) == 0)
    targets[pathId] = new DenseSet<DynInstr *>;
  dynInstr->setTaken(reason);
  targets[pathId]->insert(dynInstr);
  setTargetMask(dynInstr,  mask);
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

void TargetMgr::printTargets(void *pathId, const char *tag) {
  std::map<size_t, DynInstr *> targetMap;
  if (!hasTarget(pathId))
    return;
  errs() << BAN;
  llvm::DenseSet<DynInstr *> *pathTargets = targets[pathId];
  llvm::DenseSet<DynInstr *>::iterator itr = pathTargets->begin();
  for (; itr != pathTargets->end(); ++itr) {
    DynInstr *dynInstr = *itr;
    targetMap[dynInstr->getIndex()] = dynInstr;
  }

  std::map<size_t, DynInstr *>::iterator mapItr;
  for (mapItr = targetMap.begin(); mapItr != targetMap.end(); ++mapItr)
    stat->printDynInstr(mapItr->second, tag);
  errs() << BAN;
}

size_t TargetMgr::sizeOfTargets(void *pathId) {
  if (!hasTarget(pathId))
    return 0;
  else
    return targets[pathId]->size();
}

