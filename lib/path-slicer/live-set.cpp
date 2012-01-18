#include "live-set.h"
#include "type-defs.h"
#include "macros.h"
using namespace tern;

LiveSet::LiveSet() {
  clear();
}

LiveSet::~LiveSet() {

}

size_t LiveSet::virtRegsSize() {
  return virtRegs.size();
}

void LiveSet::clear() {
  virtRegs.clear();
  loadInstrs.clear();
  loadInstrsBDD = bddfalse;
}

void LiveSet::addReg(DynOprd *dynOprd) {

}

void LiveSet::delReg(DynOprd *dynOprd) {
  CtxVPair p = std::make_pair(
    dynOprd->getDynInstr()->getCallingCtx(), 
    dynOprd->getStaticValue());
  ASSERT(DS_IN(p, virtRegs));
  virtRegs.erase(p);
}

CtxVDenseSet &LiveSet::getAllRegs() {
  return virtRegs;
}

bool LiveSet::regIn(DynOprd *dynOprd) {
  CtxVPair p = std::make_pair(
    dynOprd->getDynInstr()->getCallingCtx(), 
    dynOprd->getStaticValue());
  return (DS_IN(p, virtRegs));
}

void LiveSet::addUsedRegs(DynInstr *dynInstr) {

}

void LiveSet::addLoadMem(DynInstr *dynInstr) {

}

void LiveSet::delLoadMem(DynInstr *dynInstr) {
  
}

DenseSet<DynInstr *> &LiveSet::getAllLoadInstrs() {
  return loadInstrs;
  /*CtxVDenseSet::iterator itr(virtRegs.begin());
  for (; itr != virtRegs.end(); ++itr) {
    
  }*/
}

const bdd LiveSet::getAllLoadMem() {
  return loadInstrsBDD;
}


