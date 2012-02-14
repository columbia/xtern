#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "intra-slicer.h"
#include "util.h"
#include "macros.h"
#include "path-slicer.h"
using namespace tern;

using namespace klee;

IntraSlicer::IntraSlicer() {

}

IntraSlicer::~IntraSlicer() {}

/* Core function for intra-thread slicer. */
void IntraSlicer::detectInputDepRaces(uchar tid) {
  fprintf(stderr, "%sIntraSlicer::detectInputDepRaces tid %u, start index " SZ "\n",
    BAN, tid, curIndex);
  DynInstr *cur = NULL;
  while (!empty()) {
    cur = delTraceTail(tid);

    if (!cur)
      return;
  
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

    //stat->printDynInstr(cur, __func__);
  }
}

bool IntraSlicer::empty() {
  return curIndex == SIZE_T_INVALID;
}

DynInstr *IntraSlicer::delTraceTail(uchar tid) {
  ASSERT(curIndex < trace->size());
  DynInstr *dynInstr = NULL;
  while (!empty()) {
    if (trace->at(curIndex)->getTid() == tid) {
      dynInstr = trace->at(curIndex);
      curIndex--;
      break;
    } else
      curIndex--;
  }
  //if (dynInstr)
  //  stat->printDynInstr(dynInstr, __func__);
  return dynInstr;
}

void IntraSlicer::init(ExecutionState *state, OprdSumm *oprdSumm, FuncSumm *funcSumm,
      AliasMgr *aliasMgr, InstrIdMgr *idMgr, CfgMgr *cfgMgr, Stat *stat,
      const DynInstrVector *trace, size_t startIndex) {
  this->state = state;
  this->oprdSumm = oprdSumm;
  this->funcSumm = funcSumm;
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
  this->cfgMgr = cfgMgr;
  this->stat = stat;
  this->trace = trace;
  curIndex = startIndex;
  live.clear();
  slice.clear();
  live.init(aliasMgr, idMgr);
}

void IntraSlicer::takeNonMem(DynInstr *dynInstr, uchar reason) {
  delRegOverWritten(dynInstr);
  live.addUsedRegs(dynInstr);
  slice.add(dynInstr, reason);
}

void IntraSlicer::delRegOverWritten(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::hasDestOprd(instr)) {
    DynOprd destOprd(dynInstr, Util::getDestOprd(instr), -1);
    live.delReg(&destOprd);
  }
}

bool IntraSlicer::regOverWritten(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::hasDestOprd(instr)) {
    DynOprd destOprd(dynInstr, Util::getDestOprd(instr), -1);
    return live.regIn(&destOprd);
  }
  return false;
}

bool IntraSlicer::retRegOverWritten(DynRetInstr *dynRetInstr) {
  DynInstr *callDynInstr = (DynInstr *)(dynRetInstr->getDynCallInstr());
  if (!callDynInstr)  // If this is a return instruction of main() or thread routine, return false.
    return false;
  return regOverWritten(callDynInstr);
}

bool IntraSlicer::eventBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr) {
  Instruction *prevInstr = idMgr->getOrigInstr((DynInstr *)dynBrInstr);
  BranchInst *branch = dyn_cast<BranchInst>(prevInstr);
  assert(branch);
  Instruction *postInstr = idMgr->getOrigInstr((DynInstr *)dynPostInstr);
  return funcSumm->eventBetween(branch, postInstr);
}

bool IntraSlicer::writtenBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr) {
  bdd bddBetween = bddfalse;
  oprdSumm->getStoreSummBetween((DynInstr *)dynBrInstr, dynPostInstr, bddBetween);
  const bdd bddOfLive = live.getAllLoadMem();
  return (bddBetween & bddOfLive) != bddfalse;
}

bool IntraSlicer::mayWriteFunc(DynRetInstr *dynRetInstr) {
  DynCallInstr *dynCallInstr = dynRetInstr->getDynCallInstr();
  if (!dynCallInstr)
    return false;
  bdd bddOfCall = bddfalse;
  oprdSumm->getStoreSummInFunc(dynCallInstr, bddOfCall);
  const bdd bddOfLive = live.getAllLoadMem();
  return (bddOfCall & bddOfLive) != bddfalse;
}

bool IntraSlicer::mayCallEvent(DynRetInstr *dynRetInstr) {
  DynCallInstr *dynCallInstr = dynRetInstr->getDynCallInstr();
  if (!dynCallInstr)
    return false;
  return funcSumm->mayCallEvent(dynCallInstr);
}

void IntraSlicer::handlePHI(DynInstr *dynInstr) {
  DynPHIInstr *phiInstr = (DynPHIInstr*)dynInstr;
  if (regOverWritten(phiInstr)) {
    delRegOverWritten(phiInstr);
    uchar index = phiInstr->getIncomingIndex();
    PHINode *phiNode = dyn_cast<PHINode>(idMgr->getOrigInstr(dynInstr));
    DynOprd oprd(phiInstr, phiNode->getIncomingValue(index), index);
    if (!Util::isConstant(&oprd)) {
      live.addReg(&oprd);
    } else {
      DynInstr *prevInstr = prevDynInstr(phiInstr);
      prevInstr->setTaken(INTRA_PHI_BR_CTRL_DEP);
    }
    slice.add(phiInstr, INTRA_PHI);
  }
}

void IntraSlicer::handleBranch(DynInstr *dynInstr) {
  DynBrInstr *brInstr = (DynBrInstr*)dynInstr;
  if (brInstr->isTaken() || brInstr->isTarget()) {
    takeNonMem(brInstr);
  } else {
    DynInstr *head = slice.getHead();
    if (!postDominate(head, brInstr) || eventBetween(brInstr, head) ||
      writtenBetween(brInstr, head)) {
      takeNonMem(brInstr);
    }
  }
}

void IntraSlicer::handleRet(DynInstr *dynInstr) {
  DynRetInstr *retInstr = (DynRetInstr*)dynInstr;
  if (retRegOverWritten(retInstr)) {
    delRegOverWritten(retInstr);
    live.addUsedRegs(retInstr);
    slice.add(retInstr, INTRA_RET_REG_OW);
  } else {
    bool reason1 = mayCallEvent(retInstr);
    bool reason2 = mayWriteFunc(retInstr);
    if (reason1 && reason2)
      slice.add(retInstr, INTRA_RET_BOTH);
    else if (reason1 && !reason2)
      slice.add(retInstr, INTRA_RET_CALL_EVENT);
    else if (!reason1 && reason2)
      slice.add(retInstr, INTRA_RET_WRITE_FUNC);
    else
      removeRange(retInstr);
  }
}

void IntraSlicer::handleCall(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  if (funcSumm->isInternalCall(callInstr)) {
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
      DynOprd loadPtrOprd(loadInstr, instr->getOperand(0), 0);
      live.addReg(&loadPtrOprd);
      live.addLoadMem(loadInstr);
      if (reason1 /* no matter whether reason2 is true */)
        slice.add(loadInstr, INTER_LOAD_TGT);
      else if (reason2)
        slice.add(loadInstr, INTRA_LOAD_OW);
    }
  } else {
    DynMemInstr *storeInstr = (DynMemInstr*)dynInstr;
    DynOprd storeValue(storeInstr, instr->getOperand(0), 0);
    DynOprd storePtrOprd(storeInstr, instr->getOperand(1), 1);
    if (storeInstr->isTarget()) {
      live.addReg(&storePtrOprd);
      slice.add(storeInstr, INTER_STORE_TGT);
    }
    DenseSet<DynInstr *> loadInstrs = live.getAllLoadInstrs();
    DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());
    for (; itr != loadInstrs.end(); ++itr) {
      DynMemInstr *loadInstr = (DynMemInstr*)(*itr);
      Instruction *staticLoadInstr = idMgr->getOrigInstr((DynInstr *)loadInstr);
      DynOprd loadPtrOprd(loadInstr, staticLoadInstr->getOperand(0), 0);
      if (mustAlias(&loadPtrOprd, &storePtrOprd)) {
        if (loadInstr->isAddrSymbolic() || storeInstr->isAddrSymbolic()) {
          addMemAddrEqConstr(loadInstr, storeInstr);
        }
        live.delLoadMem(loadInstr);
        live.addReg(&storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(&storePtrOprd);
          slice.add(storeInstr, INTRA_STORE_OW);
        }
      } else if (aliasMgr->mayAlias(&loadPtrOprd, &storePtrOprd)) {
        live.addReg(&storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(&storePtrOprd);
          slice.add(storeInstr, INTRA_STORE_ALIAS);
        }
      }
    }
  }
}

bool IntraSlicer::mustAlias(DynOprd *oprd1, DynOprd *oprd2) {
  const Value *v1 = oprd1->getStaticValue();
  const Value *v2 = oprd2->getStaticValue();
  if (v1 == v2) {
    if (Util::isConstant(v1) || isa<AllocaInst>(v1))
      return true;
  }
  return false;
}

DynInstr *IntraSlicer::prevDynInstr(DynInstr *dynInstr) {
  uchar tid = dynInstr->getTid();
  size_t tmpIndex = curIndex;
  DynInstr *prevInstr = NULL;
  while (tmpIndex != SIZE_T_INVALID) {
    if (trace->at(tmpIndex)->getTid() == tid) {
      prevInstr = trace->at(tmpIndex);
      break;
    } else
      tmpIndex--;
  }
  return prevInstr;
}

bool IntraSlicer::postDominate(DynInstr *dynPostInstr, DynInstr *dynPrevInstr) {
  Instruction *prevInstr = idMgr->getOrigInstr(dynPrevInstr);
  Instruction *postInstr = idMgr->getOrigInstr(dynPostInstr);
  bool result = cfgMgr->postDominate(prevInstr, postInstr);

  // DEBUG. This assertion does not hold, because we can have loops.
  if (Util::getBasicBlock(prevInstr) == Util::getBasicBlock(postInstr)) {
    stat->printDynInstr(dynPrevInstr, "IntraSlicer::postDominate");
    stat->printDynInstr(dynPostInstr, "IntraSlicer::postDominate");
    calStat();
    fprintf(stderr, "IntraSlicer::postDominate result %d\n", result);
    //assert(false);  
  }

  return result;
}

void IntraSlicer::removeRange(DynRetInstr *dynRetInstr) {
  size_t callIndex = 0;
  DynCallInstr *call = dynRetInstr->getDynCallInstr();
  if (call)
    callIndex = call->getIndex();
  while (curIndex != callIndex) {
    // DEBUG.
    if (trace->at(curIndex)->isTaken()) {
      stat->printDynInstr(dynRetInstr, "IntraSlicer::removeRange");
      calStat();
      assert(false);
    }
    curIndex--;
  }
  assert(!trace->at(curIndex)->isTaken());
  curIndex--;
}

void IntraSlicer::addMemAddrEqConstr(DynMemInstr *loadInstr,
  DynMemInstr *storeInstr) {
  // Shall we add the constraint to constraints of current ExecutionState?
}

void IntraSlicer::takeStartTarget(DynInstr *dynInstr) {
  slice.add(dynInstr, START_TARGET);
  curIndex = dynInstr->getIndex()-1;
}

void IntraSlicer::addDynOprd(DynOprd *dynOprd) {
  // TBD.
}

void IntraSlicer::calStat() {
  size_t numExedInstrs = trace->size();
  size_t numTakenInstrs = slice.size();
  size_t numExedBrs = 0;
  size_t numExedSymBrs = 0;
  size_t numTakenBrs = 0;
  size_t numTakenSymBrs = 0;
  
  for (size_t i = 0; i < trace->size(); i++) {
    DynInstr *dynInstr = trace->at(i);
    Instruction *instr = idMgr->getOrigInstr(dynInstr);

    // Handle branches.
    if (Util::isBr(instr)) {
      numExedBrs++;
      if (!Util::isUniCondBr(instr)) {
        DynBrInstr *br = (DynBrInstr *)dynInstr;
        if (br->isSymbolicBr())
          numExedSymBrs++;
      }
    }
    
    if (dynInstr->isTaken()) {
      // Handle branches.
      if (Util::isBr(instr)) {
        numTakenBrs++;
        if (!Util::isUniCondBr(instr)) {
          DynBrInstr *br = (DynBrInstr *)dynInstr;
          if (br->isSymbolicBr())
            numTakenSymBrs++;
        }
      }
      
      stat->printDynInstr(dynInstr, "IntraSlicer::calStat TAKEN");
    }
  }

  errs() << "\n\nIntraSlicer::calStat STATISTICS"
    << ": numExedInstrs: " << numExedInstrs
    << ";  numTakenInstrs: " << numTakenInstrs
    << ";  numExedBrs: " << numExedBrs
    << ";  numTakenBrs: " << numTakenBrs
    << ";  numExedSymBrs: " << numExedSymBrs
    << ";  numTakenSymBrs: " << numTakenSymBrs
    << ".\n\n\n";
    
}

