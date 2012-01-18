#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "intra-slicer.h"
#include "util.h"
#include "macros.h"
using namespace tern;

IntraSlicer::IntraSlicer() {

}

IntraSlicer::~IntraSlicer() {}

/* Core function for intra-thread slicer. */
void IntraSlicer::detectInputDepRaces() {
  DynInstr *cur = NULL;
  while (!empty()) {
    cur = delTraceTail();
    if (Util::isPHI(cur)) {
      handlePHI(cur);
    } else if (Util::isBr(cur)) {
      handleBranch(cur);
    } else if (Util::isRet(cur)) {
      handleRet(cur);
    } else if (Util::isCall(cur)) { /* Invoke is lowered to call. */
      handleCall(cur);
    } else if (Util::isMem(cur)) {
      handleMem(cur);
    } else { /* Handle all the other non-memory instructions. */
      handleNonMem(cur);
    }
  }
}

bool IntraSlicer::empty() {
  return curIndex != SIZE_T_INVALID;
}

DynInstr *IntraSlicer::delTraceTail() {
  ASSERT(!empty() && curIndex < trace->size());
  DynInstr *dynInstr = trace->at(curIndex);
  curIndex--;
  return dynInstr;
}

void IntraSlicer::init(const DynInstrVector *trace, size_t startIndex) {
  this->trace = trace;
  curIndex = startIndex;
  live.clear();
}

void IntraSlicer::takeNonMem(DynInstr *dynInstr) {
  delRegOverWritten(dynInstr);
  live.addUsedRegs(dynInstr);
  slice->add(dynInstr, __func__);
}

void IntraSlicer::delRegOverWritten(DynInstr *dynInstr) {
  live.delReg(dynInstr->getDestOprd());
}

bool IntraSlicer::regOverWritten(DynInstr *dynInstr) {
  return live.regIn(dynInstr->getDestOprd());
}

DynInstr *IntraSlicer::getCallInstrWithRet(DynInstr *retDynInstr) {
  return NULL;
}

bool IntraSlicer::retRegOverWritten(DynInstr *dynInstr) {
  DynInstr *callDynInstr = getCallInstrWithRet(dynInstr);
  return regOverWritten(callDynInstr);
}

bool IntraSlicer::eventBetween(DynInstr *dynInstr) {
  return false;
}

bool IntraSlicer::writtenBetween(DynInstr *dynInstr) {
  return false;
}

bool IntraSlicer::mayWriteFunc(DynInstr *dynInstr, Function *func) {
  return false;
}

bool IntraSlicer::mayCallEvent(DynInstr *dynInstr, Function *func) {
  return false;
}

void IntraSlicer::handlePHI(DynInstr *dynInstr) {
  DynPHIInstr *phiInstr = (DynPHIInstr*)dynInstr;
  if (regOverWritten(phiInstr)) {
    delRegOverWritten(phiInstr);
    unsigned index = phiInstr->getIncomingIndex();
    DynOprd *oprd = phiInstr->getUsedOprd(index);
    if (!oprd->isConstant()) {
      live.addReg(oprd);
    } else {
      DynInstr *prevInstr = prevDynInstr(phiInstr);
      prevInstr->setTaken(true, __func__);
    }
    slice->add(phiInstr, __func__);
  }
}

void IntraSlicer::handleBranch(DynInstr *dynInstr) {
  DynBrInstr *brInstr = (DynBrInstr*)dynInstr;
  if (brInstr->isTaken() || brInstr->isTarget()) {
    takeNonMem(brInstr);
  } else {
    DynInstr *head = slice->getHead();
    if (!postDominate(head, brInstr) || eventBetween(brInstr) ||
      writtenBetween(brInstr)) {
      takeNonMem(brInstr);
    }
  }
}

void IntraSlicer::handleRet(DynInstr *dynInstr) {
  DynRetInstr *retInstr = (DynRetInstr*)dynInstr;
  Instruction *instr = retInstr->getOrigInstr();
  Function *calledFunc = instr->getParent()->getParent();
  if (retRegOverWritten(retInstr)) {
    delRegOverWritten(retInstr);
    live.addUsedRegs(retInstr);
    slice->add(retInstr, __func__);
  } else if (mayCallEvent(retInstr, calledFunc) || 
  mayWriteFunc(retInstr, calledFunc)) {
    slice->add(retInstr, __func__);
  } else {
    removeRange(retInstr);
  }
}

void IntraSlicer::handleCall(DynInstr *dynInstr) {
  // TBD.
}

void IntraSlicer::handleNonMem(DynInstr *dynInstr) {
  if (regOverWritten(dynInstr))
    takeNonMem(dynInstr);
}

void IntraSlicer::handleMem(DynInstr *dynInstr) {
  if (Util::isLoad(dynInstr)) {
    DynMemInstr *loadInstr = (DynMemInstr*)dynInstr;
    if (loadInstr->isTarget() || regOverWritten(loadInstr)) {
      delRegOverWritten(loadInstr);
      live.addReg(loadInstr->getUsedOprd(0));
      live.addLoadMem(loadInstr);
      slice->add(loadInstr, __func__);
    }
  } else {
    DynMemInstr *storeInstr = (DynMemInstr*)dynInstr;
    DynOprd *storeValue = storeInstr->getUsedOprd(0);
    DynOprd *storePtrOprd = storeInstr->getUsedOprd(1);
    if (storeInstr->isTarget()) {
      live.addReg(storePtrOprd);
      slice->add(storeInstr, __func__);
    }
    DenseSet<DynInstr *> loadInstrs = live.getAllLoadInstrs();
    DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());
    for (; itr != loadInstrs.end(); ++itr) {
      DynMemInstr *loadInstr = (DynMemInstr*)(*itr);
      DynOprd *loadPtrOprd = loadInstr->getUsedOprd(0);
      if (loadInstr->getMemAddr() == storeInstr->getMemAddr()) {
        if (loadInstr->isMemAddrSymbolic() || storeInstr->isMemAddrSymbolic()) {
          addMemAddrEqConstr(loadInstr, storeInstr);
        }
        live.delLoadMem(loadInstr);
        live.addReg(storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(storePtrOprd);
          slice->add(storeInstr, __func__);
        }
      } else if (aliasMgr->mayAlias(loadPtrOprd, storePtrOprd)) {
        live.addReg(storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(storePtrOprd);
          slice->add(storeInstr, __func__);
        }
      }
    }

  }
}

DynInstr *IntraSlicer::prevDynInstr(DynInstr *dynInstr) {
  return NULL;
}

bool IntraSlicer::postDominate(DynInstr *dynPostInstr, DynInstr *dynPrevInstr) {
  return false;
}

void IntraSlicer::removeRange(DynInstr *dynInstr) {

}

void IntraSlicer::addMemAddrEqConstr(DynMemInstr *loadInstr,
  DynMemInstr *storeInstr) {

}


