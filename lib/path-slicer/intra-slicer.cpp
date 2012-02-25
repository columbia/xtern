#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "intra-slicer.h"
#include "util.h"
#include "path-slicer.h"
using namespace tern;

using namespace klee;

IntraSlicer::IntraSlicer() {

}

IntraSlicer::~IntraSlicer() {}

/* Core function for intra-thread slicer. */
void IntraSlicer::detectInputDepRaces(uchar tid) {
  dprint("%sIntraSlicer::detectInputDepRaces tid %u, start index " SZ "\n", BAN, tid, curIndex);
  DynInstr *cur = NULL;
  while (!empty()) {
    cur = delTraceTail(tid);

    if (!cur)
      return;

    // Handle checker targets.
    if (cur->isCheckerTarget()) {
      handleCheckerTarget(cur);
      continue;
    }

    // Do slicing.
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
  return curIndex == SIZE_T_INVALID;
}

DynInstr *IntraSlicer::delTraceTail(uchar tid) {
  if (curIndex >= trace->size()) {
    fprintf(stderr, "IntraSlicer::delTraceTail " SZ ", " SZ "\n", curIndex, trace->size());
    assert(false);
  }
  DynInstr *dynInstr = NULL;
  while (!empty()) {
    if (trace->at(curIndex)->getTid() == tid) {
      dynInstr = trace->at(curIndex);
      curIndex--;
      break;
    } else
      curIndex--;
  }
  if (dynInstr && DBG)
    stat->printDynInstr(dynInstr, __func__);
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
  live.init(aliasMgr, idMgr, stat);
}

void IntraSlicer::takeNonMem(DynInstr *dynInstr, uchar reason) {
  delRegOverWritten(dynInstr);
  live.addUsedRegs(dynInstr);
  slice.add(dynInstr, reason);
}

void IntraSlicer::takeBr(DynInstr *dynInstr, uchar reason) {
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
  DynInstr *dynCallInstr = (DynInstr *)(dynRetInstr->getDynCallInstr());
  assert(dynCallInstr);
  return regOverWritten(dynCallInstr);
}

bool IntraSlicer::eventBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr) {
  Instruction *prevInstr = idMgr->getOrigInstr((DynInstr *)dynBrInstr);
  BranchInst *branch = dyn_cast<BranchInst>(prevInstr);
  assert(branch);
  Instruction *postInstr = NULL;
  if (dynPostInstr) /* dynPostInstr can be NULL because sometimes we start from empty target. */
    postInstr = idMgr->getOrigInstr((DynInstr *)dynPostInstr);
  else {
    postInstr = cfgMgr->getStaticPostDom(prevInstr);
    /*if (DBG && postInstr)
      errs() << "IntraSlicer::eventBetween: " << *(postInstr) << "\n";*/
  }
  return funcSumm->eventBetween(branch, postInstr);
}

bool IntraSlicer::writtenBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr) {
  /* dynPostInstr or successor can be NULL because sometimes we start from empty target. */
  if (!dynPostInstr || !dynBrInstr->getSuccessorBB()) {  
    assert(slice.size() == 0 && live.virtRegsSize() == 0 && live.loadInstrsSize() == 0);
    return false;
  }
  bdd bddBetween = bddfalse;
  oprdSumm->getStoreSummBetween(dynBrInstr, dynPostInstr, bddBetween);
  const bdd bddOfLive = live.getAllLoadMem();
  return (bddBetween & bddOfLive) != bddfalse;
}

bool IntraSlicer::mayWriteFunc(DynRetInstr *dynRetInstr) {
  bdd bddOfCall = bddfalse;
  oprdSumm->getStoreSummInFunc(dynRetInstr, bddOfCall);
  const bdd bddOfLive = live.getAllLoadMem();
  return (bddOfCall & bddOfLive) != bddfalse;
}

bool IntraSlicer::mayCallEvent(DynRetInstr *dynRetInstr) {
  DynCallInstr *dynCallInstr = dynRetInstr->getDynCallInstr();
  if (!dynCallInstr)
    return false;
  return funcSumm->mayCallEvent(dynCallInstr);
}

void IntraSlicer::handleCheckerTarget(DynInstr *dynInstr) {
  if (regOverWritten(dynInstr))
    delRegOverWritten(dynInstr);
  live.addUsedRegs(dynInstr);
  slice.add(dynInstr, dynInstr->getTakenFlag());
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
      prevInstr->setTaken(TakenFlags::INTRA_PHI_BR_CTRL_DEP);
    }
    slice.add(phiInstr, TakenFlags::INTRA_PHI);
  }
}

void IntraSlicer::handleBranch(DynInstr *dynInstr) {
  DynBrInstr *brInstr = (DynBrInstr*)dynInstr;
  /* A branch can be taken already by considering control dependency in 
  handling phi instructions, or taken already by considering br-br may race in 
  inter-thread phase. */
  if (brInstr->isTaken() || brInstr->isInterThreadTarget()) {
    takeBr(brInstr, brInstr->getTakenFlag());
  } else {
    DynInstr *head = slice.getHead();
    if (!postDominate(head, brInstr)) {
      takeBr(brInstr, TakenFlags::INTRA_BR_N_POSTDOM);
      return;
    }
    if (eventBetween(brInstr, head)) {
      takeBr(brInstr, TakenFlags::INTRA_BR_EVENT_BETWEEN);
      return;
    }
    if (writtenBetween(brInstr, head)) {
      takeBr(brInstr, TakenFlags::INTRA_BR_WR_BETWEEN);
      return;
    }
   }
}

void IntraSlicer::handleRet(DynInstr *dynInstr) {
  DynRetInstr *retInstr = (DynRetInstr*)dynInstr;
  /* If current return instruction does not have a call instruction, it must 
  be the return of main() of thread routines, if it is target, it is already 
  taken by handle*Target(); if it is not target, it should not be taken, but we 
  still should not remove the instruction range for current funcion, we just 
  simply return. */
  if (!retInstr->getDynCallInstr())
    return;
  if (retRegOverWritten(retInstr)) {
    delRegOverWritten(retInstr);
    live.addUsedRegs(retInstr);
    slice.add(retInstr, TakenFlags::INTRA_RET_REG_OW);
  } else {
    bool reason1 = mayCallEvent(retInstr);
    bool reason2 = mayWriteFunc(retInstr);
    SERRS << "IntraSlicer::handleRet reason1 " << reason1
      << ", reason2 " << reason2 << ".\n";
    if (reason1 && reason2)
      slice.add(retInstr, TakenFlags::INTRA_RET_BOTH);
    else if (reason1 && !reason2)
      slice.add(retInstr, TakenFlags::INTRA_RET_CALL_EVENT);
    else if (!reason1 && reason2)
      slice.add(retInstr, TakenFlags::INTRA_RET_WRITE_FUNC);
    else
      removeRange(retInstr);
  }
}

void IntraSlicer::handleCall(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  if (funcSumm->isInternalCall(callInstr)) {
    if (regOverWritten(dynInstr))
      delRegOverWritten(dynInstr);
    live.addUsedRegs(dynInstr);
    // Currently set the reason to non mem, real reason depends on its return instr.
    slice.add(dynInstr, TakenFlags::INTRA_NON_MEM); 
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
    bool reason1 = loadInstr->isInterThreadTarget();
    bool reason2 = regOverWritten(loadInstr);
    SERRS << "IntraSlicer::handleMem reason1 " 
      << reason1 << ", reason2 " << reason2 << "\n";
    if (reason1 || reason2) {
      if (reason2)
        delRegOverWritten(loadInstr);
      DynOprd loadPtrOprd(loadInstr, instr->getOperand(0), 0);
      live.addReg(&loadPtrOprd);
      live.addLoadMem(loadInstr);
      if (reason1 /* no matter whether reason2 is true */)
        slice.add(loadInstr, TakenFlags::INTER_LOAD_TGT);
      else if (reason2)
        slice.add(loadInstr, TakenFlags::INTRA_LOAD_OW);
    }
  } else {
    DynMemInstr *storeInstr = (DynMemInstr*)dynInstr;
    DynOprd storeValue(storeInstr, instr->getOperand(0), 0);
    DynOprd storePtrOprd(storeInstr, instr->getOperand(1), 1);
    if (storeInstr->isInterThreadTarget()) {
      live.addReg(&storePtrOprd);
      slice.add(storeInstr, TakenFlags::INTER_STORE_TGT);
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
          slice.add(storeInstr, TakenFlags::INTRA_STORE_OW);
        }
      } else if (aliasMgr->mayAlias(&loadPtrOprd, &storePtrOprd)) {
        live.addReg(&storeValue);
        if (!storeInstr->isTaken()) {
          live.addReg(&storePtrOprd);
          slice.add(storeInstr, TakenFlags::INTRA_STORE_ALIAS);
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
  //if (Util::isErrnoAddr(v1) && Util::isErrnoAddr(v2))
    //return true;
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

bool IntraSlicer::postDominate(DynInstr *dynPostInstr, DynBrInstr *dynPrevInstr) {
  /* dynPostInstr or successor can be NULL because sometimes we start from empty target. */
  if (!dynPostInstr || !dynPrevInstr->getSuccessorBB()) {
    assert(slice.size() == 0);
    return true;
  }
  Instruction *prevInstr = idMgr->getOrigInstr(dynPrevInstr);
  Instruction *postInstr = idMgr->getOrigInstr(dynPostInstr);
  bool result = cfgMgr->postDominate(prevInstr, postInstr);

  // DEBUG. This assertion does not hold, because we can have loops.
  if (Util::getBasicBlock(prevInstr) == Util::getBasicBlock(postInstr)) {
    stat->printDynInstr(dynPrevInstr, "IntraSlicer::postDominate");
    stat->printDynInstr(dynPostInstr, "IntraSlicer::postDominate");
    fprintf(stderr, "IntraSlicer::postDominate result %d\n", result);
    //assert(false);  
  }

  return result;
}

void IntraSlicer::removeRange(DynRetInstr *dynRetInstr) {
  DynCallInstr *call = dynRetInstr->getDynCallInstr();
  assert(call);
  /* Directly sets the current slicing index to be the previous one of the call instr. */
  curIndex = call->getIndex() - 1;
  assert(!empty()); 
}

void IntraSlicer::addMemAddrEqConstr(DynMemInstr *loadInstr,
  DynMemInstr *storeInstr) {
  // Shall we add the constraint to constraints of current ExecutionState?
}

/* Even for store instructions, we still need to take the store pointer 
operand, because it has meaningful effect for this instruction (the stored 
target memory location). */
void IntraSlicer::takeTestTarget(DynInstr *dynInstr) {
  live.addUsedRegs(dynInstr);
  slice.add(dynInstr, TakenFlags::TEST_TARGET);
  curIndex = dynInstr->getIndex()-1;
}

void IntraSlicer::calStat(set<BranchInst *> &rmBrs) {
  std::string checkTag = LLVM_CHECK_TAG;
  checkTag += "IntraSlicer::calStat TAKEN";
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

    // Handle not-taken bi-cond branches, pass to argument.
    if (!dynInstr->isTaken() && Util::isBr(instr) && !Util::isUniCondBr(instr))
      rmBrs.insert(cast<BranchInst>(instr));

    // Handle taken instructions.
    if (dynInstr->isTaken()) {
      // Handle taken branches.
      if (Util::isBr(instr)) {
        numTakenBrs++;
        if (!Util::isUniCondBr(instr)) {
          DynBrInstr *br = (DynBrInstr *)dynInstr;
          if (br->isSymbolicBr())
            numTakenSymBrs++;
        }
      }

      if (DBG)
        stat->printDynInstr(dynInstr, checkTag.c_str());
    }
  }

  errs() << "\n\n" << LLVM_CHECK_TAG
    << "IntraSlicer::calStat STATISTICS"
    << ": numExedInstrs: " << numExedInstrs
    << ";  numTakenInstrs: " << numTakenInstrs
    << ";  numExedBrs: " << numExedBrs
    << ";  numTakenBrs: " << numTakenBrs
    << ";  numExedSymBrs: " << numExedSymBrs
    << ";  numTakenSymBrs: " << numTakenSymBrs
    << ". StaticExed/Static Instrs: " << stat->sizeOfExedStaticInstrs() 
    << "/" << stat->sizeOfStaticInstrs()
    << ".\n\n\n";

  stat->printExternalCalls();
}

