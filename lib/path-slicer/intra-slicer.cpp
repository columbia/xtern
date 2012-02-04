#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "intra-slicer.h"
#include "util.h"
#include "macros.h"
#include "path-slicer.h"
using namespace tern;

IntraSlicer::IntraSlicer() {

}

IntraSlicer::~IntraSlicer() {}

/* Core function for intra-thread slicer. */
void IntraSlicer::detectInputDepRaces() {
  DynInstr *cur = NULL;
  while (!empty()) {
    cur = delTraceTail();
    Instruction *instr = idMgr->getOrigInstr(cur);
    if (Util::isPHI(instr)) {
      handlePHI(cur);
    } else if (Util::isBr(instr)) {
      handleBranch(cur);
    } else if (Util::isRet(instr)) {
      handleRet(cur);
    } else if (Util::isCall(instr)) { /* Invoke is lowered to call. */
      handleCall(cur);
    } else if (Util::isMem(instr)) {
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

void IntraSlicer::init(PathSlicer *pathSlicer, InstrIdMgr *idMgr, 
  const DynInstrVector *trace, size_t startIndex) {
  this->pathSlicer = pathSlicer;
  this->idMgr = idMgr;
  this->trace = trace;
  curIndex = startIndex;
  live.clear();
  live.init(aliasMgr, idMgr);
}

void IntraSlicer::takeNonMem(DynInstr *dynInstr, uchar reason) {
  delRegOverWritten(dynInstr);
  live.addUsedRegs(dynInstr);
  slice->add(dynInstr, reason);
}

void IntraSlicer::delRegOverWritten(DynInstr *dynInstr) {
  DynOprd destOprd(dynInstr, -1);
  live.delReg(&destOprd);
}

bool IntraSlicer::regOverWritten(DynInstr *dynInstr) {
  DynOprd destOprd(dynInstr, -1);
  return live.regIn(&destOprd);
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
    DynOprd oprd(phiInstr, index);
    if (oprd.isConstant()) {
      live.addReg(&oprd);
    } else {
      DynInstr *prevInstr = prevDynInstr(phiInstr);
      prevInstr->setTaken(INTRA_PHI_BR_CTRL_DEP);
    }
    slice->add(phiInstr, INTRA_PHI);
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
  Instruction *instr = idMgr->getOrigInstr(retInstr);
  Function *calledFunc = instr->getParent()->getParent();
  if (retRegOverWritten(retInstr)) {
    delRegOverWritten(retInstr);
    live.addUsedRegs(retInstr);
    slice->add(retInstr, INTRA_RET_REG_OW);
  } else {
    bool reason1 = mayCallEvent(retInstr, calledFunc);
    bool reason2 = mayWriteFunc(retInstr, calledFunc);
    if (reason1 && reason2)
      slice->add(retInstr, INTRA_RET_BOTH);
    else if (reason1 && !reason2)
      slice->add(retInstr, INTRA_RET_CALL_EVENT);
    else if (!reason1 && reason2)
      slice->add(retInstr, INTRA_RET_WRITE_FUNC);
    else
      removeRange(retInstr);
  }
}

void IntraSlicer::handleCall(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  if (pathSlicer->isInternalCall(callInstr)) {
    takeNonMem(callInstr);
  } else {
    // TBD: QUERY FUNCTION SUMMARY.
  }
}

void IntraSlicer::handleNonMem(DynInstr *dynInstr) {
  if (regOverWritten(dynInstr))
    takeNonMem(dynInstr);
}

void IntraSlicer::handleMem(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isLoad(instr)) {
    DynMemInstr *loadInstr = (DynMemInstr*)dynInstr;
    bool reason1 = loadInstr->isTarget();
    bool reason2 = regOverWritten(loadInstr);
    if (reason1 || reason2) {
      if (reason2)
        delRegOverWritten(loadInstr);
      DynOprd loadPtrOprd(loadInstr, 0);
      live.addReg(&loadPtrOprd);
      live.addLoadMem(loadInstr);
      if (reason1 /* no matter whether reason2 is true */)
        slice->add(loadInstr, INTER_LOAD_TGT);
      else if (reason2)
        slice->add(loadInstr, INTRA_LOAD_OW);
    }
  } else {
    DynMemInstr *storeInstr = (DynMemInstr*)dynInstr;
    DynOprd storeValue(storeInstr, 0);
    DynOprd storePtrOprd(storeInstr, 1);
    if (storeInstr->isTarget()) {
      live.addReg(&storePtrOprd);
      slice->add(storeInstr, INTER_STORE_TGT);
    }
    DenseSet<DynInstr *> loadInstrs = live.getAllLoadInstrs();
    DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());
    for (; itr != loadInstrs.end(); ++itr) {
      DynMemInstr *loadInstr = (DynMemInstr*)(*itr);
      DynOprd loadPtrOprd(loadInstr, 0);
      if (loadInstr->getConAddr() == storeInstr->getConAddr()) {
        if (loadInstr->isAddrSymbolic() || storeInstr->isAddrSymbolic()) {
          addMemAddrEqConstr(loadInstr, storeInstr);
        }
        live.delLoadMem(loadInstr);
        live.addReg(&storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(&storePtrOprd);
          slice->add(storeInstr, INTRA_STORE_OW);
        }
      } else if (aliasMgr->mayAlias(&loadPtrOprd, &storePtrOprd)) {
        live.addReg(&storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(&storePtrOprd);
          slice->add(storeInstr, INTRA_STORE_ALIAS);
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


