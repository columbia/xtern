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
  DynInstr *cur = NULL;
  while (!empty()) {
    cur = delTraceTail(tid);

    if (!cur)
      return;

    // Handle checker targets.
    if (cur->isCheckerTarget()) {
      handleCheckerTarget(cur);
      if (DBG)
        stat->printDynInstr(cur, __func__);
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
    if (DBG)
      stat->printDynInstr(cur, __func__);
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
  return dynInstr;
}

void IntraSlicer::init(ExecutionState *state, OprdSumm *oprdSumm, FuncSumm *funcSumm,
      AliasMgr *aliasMgr, InstrIdMgr *idMgr, CfgMgr *cfgMgr, CallStackMgr *ctxMgr, TargetMgr *tgtMgr, Stat *stat,
      const DynInstrVector *trace, size_t startIndex) {
  this->state = state;
  this->oprdSumm = oprdSumm;
  this->funcSumm = funcSumm;
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
  this->cfgMgr = cfgMgr;
  this->ctxMgr = ctxMgr;
  this->tgtMgr = tgtMgr;
  this->stat = stat;
  this->trace = trace;
  curIndex = startIndex;
  live.clear();
  slice.clear();
  live.init(aliasMgr, idMgr, stat, funcSumm);
}

void IntraSlicer::takeStore(DynInstr *dynInstr, uchar reason) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  DynMemInstr *storeInstr = (DynMemInstr*)dynInstr;
  DynOprd storeValue(storeInstr, instr->getOperand(0), 0);
  DynOprd storePtrOprd(storeInstr, instr->getOperand(1), 1);
  live.addReg(&storeValue);
  if (!storeInstr->isTaken()) {
    live.addReg(&storePtrOprd);
    slice.add(storeInstr, reason);
  }
}

void IntraSlicer::takeBr(DynInstr *dynInstr, uchar reason) {
  live.addUsedRegs(dynInstr);
  slice.add(dynInstr, reason);
}

void IntraSlicer::takeExternalCall(DynInstr *dynInstr, uchar reason) {
  if (regOverWritten(dynInstr))
    delRegOverWritten(dynInstr);
  live.addUsedRegs(dynInstr);
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (funcSumm->extFuncHasLoadSumm(instr)) {
    live.addExtCallLoadMem(dynInstr);
  }
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
    if (DBG && postInstr)
      errs() << "IntraSlicer::eventBetween postInstr: " << *(postInstr) << "\n";
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

bool IntraSlicer::phiDefBetween(DynBrInstr *dynBrInstr, DynInstr *dynPostInstr) {
  InstrDenseSet phiSet;
  oprdSumm->getUsedByPhiSummBetween(dynBrInstr, dynPostInstr, phiSet);
  return live.phiDefBetween(dynBrInstr->getCallingCtx(), &phiSet);
}

bool IntraSlicer::mayWriteFunc(DynInstr *dynInstr, DynCallInstr *caller) {
  // Passed in argument caller can be NULL, which means main().
  bdd bddOfCall = bddfalse;
  oprdSumm->getStoreSummInFunc(dynInstr, caller, bddOfCall);
  const bdd bddOfLive = live.getAllLoadMem();
  return (bddOfCall & bddOfLive) != bddfalse;
}

bool IntraSlicer::mayCallEvent(DynCallInstr *caller) {
  // If caller is NULL, means main() function, NULL is legal.
  return funcSumm->mayCallEvent(caller);
}

void IntraSlicer::addCallSiteArgs(DynInstr *dynInstr, klee::Checker::ResultType argMask) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  CallCtx *intCtx = dynInstr->getCallingCtx();
  assert(Util::isCall(instr));
  if (DBG)
     errs() << "IntraSlicer::addCallSiteArgs instr " << *instr << "\n"; 
  // Start from the first argument of the call instruction.
  for (unsigned i = 1; i < instr->getNumOperands(); i++) {
    if (i == 1 && !(argMask & Checker::NEED_ARG0))
      continue;
    if (i == 2 && !(argMask & Checker::NEED_ARG1))
      continue;
    if (i == 3 && !(argMask & Checker::NEED_ARG2))
      continue;
    if (i == 4 && !(argMask & Checker::NEED_ARG3))
      continue;
    if (i == 5 && !(argMask & Checker::NEED_ARG4))
      continue;
    if (i > 5)  // Currently all event calls have at most 4 arguments.
      abort();
    
    Value *oprd = instr->getOperand(i);
    if (DBG)
      errs() << "IntraSlicer::addCallSiteArgs instr takes oprd (" << i << ") " << *oprd << "\n"; 
    llvm::ConstantExpr *opInner = dyn_cast<llvm::ConstantExpr>(oprd);
    if(opInner) {
      if (DBG)
        errs() << "IntraSlicer::addCallSiteArgs handles nested instruction " << *instr << "\n"
          << "Oprd " << *oprd << "\n";
      live.addInnerUsedRegs(intCtx, opInner);
    } else {// Only need to add this reg when it is not a LLVM Constant.
      if (DBG)
        errs() << "IntraSlicer::addCallSiteArgs tries to add oprd " << *oprd << "\n";
      DynOprd dynOprd(dynInstr, oprd, i);
      live.addReg(&dynOprd);
    }
  }
}

void IntraSlicer::handleCheckerTarget(DynInstr *dynInstr) {
  BEGINTIME(stat->intraChkTgtSt);
  if (regOverWritten(dynInstr))
    delRegOverWritten(dynInstr);
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isCall(instr))
    addCallSiteArgs(dynInstr, tgtMgr->getTargetMask(dynInstr));
  else
    live.addUsedRegs(dynInstr);
  slice.add(dynInstr, dynInstr->getTakenFlag());
  ENDTIME(stat->intraChkTgtTime, stat->intraChkTgtSt, stat->intraChkTgtEnd);
}

void IntraSlicer::handlePHI(DynInstr *dynInstr) {
  BEGINTIME(stat->intraPhiSt);
  DynPHIInstr *phiInstr = (DynPHIInstr*)dynInstr;
  if (regOverWritten(phiInstr)) {
    delRegOverWritten(phiInstr);
    uchar index = phiInstr->getIncomingIndex();
    PHINode *phiNode = dyn_cast<PHINode>(idMgr->getOrigInstr(dynInstr));
    DynOprd oprd(phiInstr, phiNode->getIncomingValue(index), index);
    if (!Util::isConstant(&oprd)) {
      live.addReg(&oprd);
    }
    DynInstr *prevInstr = prevDynBrInstr(phiInstr);
    assert(phiNode->getIncomingBlock(index) == idMgr->getOrigInstr(prevInstr)->getParent());
    prevInstr->setTaken(TakenFlags::INTRA_PHI_BR_CTRL_DEP);
    slice.add(phiInstr, TakenFlags::INTRA_PHI);
  }
  ENDTIME(stat->intraPhiTime, stat->intraPhiSt, stat->intraPhiEnd);
}

void IntraSlicer::handleBranch(DynInstr *dynInstr) {
  BEGINTIME(stat->intraBrSt);
  DynBrInstr *brInstr = (DynBrInstr*)dynInstr;
  /* A branch can be taken already by considering control dependency in 
  handling phi instructions, or taken already by considering br-br may race in 
  inter-thread phase. */
  if (brInstr->isTaken() || brInstr->isInterThreadTarget()) {
    takeBr(brInstr, brInstr->getTakenFlag());
  } else {
    DynInstr *head = slice.getHead();

    BEGINTIME(stat->intraBrDomSt);
    bool result1 = !postDominate(head, brInstr);
    ENDTIME(stat->intraBrDomTime, stat->intraBrDomSt, stat->intraBrDomEnd);
    if (result1) {
      takeBr(brInstr, TakenFlags::INTRA_BR_N_POSTDOM);
      goto finish;
    }

    BEGINTIME(stat->intraBrEvBetSt);
    bool result2 = eventBetween(brInstr, head);
    ENDTIME(stat->intraBrEvBetTime, stat->intraBrEvBetSt, stat->intraBrEvBetEnd);
    if (result2) {
      takeBr(brInstr, TakenFlags::INTRA_BR_EVENT_BETWEEN);
      goto finish;
    }

    BEGINTIME(stat->intraBrWrBetSt);
    bool result3 = writtenBetween(brInstr, head);
    ENDTIME(stat->intraBrWrBetTime, stat->intraBrWrBetSt, stat->intraBrWrBetEnd);
    if (result3) {
      takeBr(brInstr, TakenFlags::INTRA_BR_WR_BETWEEN);
      goto finish;
    }

    bool result4 = phiDefBetween(brInstr, head);
    if (result4) {
      takeBr(brInstr, TakenFlags::INTRA_BR_PHI_DEF_BETWEEN);
      goto finish;
    }
   }

finish:
  ENDTIME(stat->intraBrTime, stat->intraBrSt, stat->intraBrEnd);
}

/*
For the same reason in handleProcessExitCall(), seems that we have to take the return instruction
even it only mayCallEvent() or mayWriteFunc(), otherwise the program may go down another path
and hits an exit(). So current implementation is correct.
foo(x) {
  if (x)
    exit(0);
  else
    return 1;
}

*/
void IntraSlicer::handleRet(DynInstr *dynInstr) {
  DynRetInstr *retInstr = (DynRetInstr*)dynInstr;
  /* If current return instruction does not have a call instruction, it must 
  be the return of main() of thread routines, if it is target, it is already 
  taken by handle*Target(); if it is not target, it should not be taken, but we 
  still should not remove the instruction range for current funcion, we just 
  simply return. */
  if (!retInstr->getDynCallInstr())
    return;
  BEGINTIME(stat->intraRetSt);
  if (retRegOverWritten(retInstr)) {
    delRegOverWritten(retInstr);
    live.addUsedRegs(retInstr);
    slice.add(retInstr, TakenFlags::INTRA_RET_REG_OW);
  } else {
    DynCallInstr *caller = retInstr->getDynCallInstr();
    bool reason1 = mayCallEvent(caller);
    bool reason2 = mayWriteFunc(retInstr, caller);
    SERRS << "IntraSlicer::handleRet reason1 " << reason1
      << ", reason2 " << reason2 << ".\n";
    if (reason1 && reason2)
      slice.add(retInstr, TakenFlags::INTRA_RET_BOTH);
    else if (reason1 && !reason2)
      slice.add(retInstr, TakenFlags::INTRA_RET_CALL_EVENT);
    else if (!reason1 && reason2)
      slice.add(retInstr, TakenFlags::INTRA_RET_WRITE_FUNC);
    else
      removeRange(dynInstr, caller);
  }
  ENDTIME(stat->intraRetTime, stat->intraRetSt, stat->intraRetEnd);
}

void IntraSlicer::handleCall(DynInstr *dynInstr) {
  BEGINTIME(stat->intraCallSt);
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;

  if (Util::isProcessExitFunc(callInstr->getCalledFunc())) {
    handleProcessExitCall(dynInstr);
    goto finished;
  }
  
  if (funcSumm->isInternalCall(callInstr)) {
    if (regOverWritten(dynInstr))
      delRegOverWritten(dynInstr);
    live.addUsedRegs(dynInstr);
    // Currently set the reason to non mem, real reason depends on its return instr.
    slice.add(dynInstr, TakenFlags::INTRA_NON_MEM); 
  } else {
    if (regOverWritten(dynInstr))
      takeExternalCall(dynInstr, TakenFlags::INTRA_EXT_CALL_REG_OW);
    else {
      Instruction *instr = idMgr->getOrigInstr(dynInstr);
      if (funcSumm->extFuncHasSumm(instr)) {
        bdd storeSumm = bddfalse;
        oprdSumm->getExtCallStoreSumm(callInstr, storeSumm);
        const bdd bddOfLive = live.getAllLoadMem();
        if ((storeSumm & bddOfLive) != bddfalse)
          takeExternalCall(dynInstr, TakenFlags::INTRA_EXT_CALL_MOD_LIVE);
      }
    }
  }

finished:
  ENDTIME(stat->intraCallTime, stat->intraCallSt, stat->intraCallEnd);
}

/*
Complicated, because exit() can be viewed as multiple return points of multiple functions,
so we have to recursively look back to callers. 
Note we have to take the exit() call, because it has effect "exit", if we do not take it, we would
go down different paths.
e.g., the foo() below, current path has an effect "exit", if we do not take this exit(), the input
may drive x to go down a different path which just return and the process goes on to run,
which has different effect from current path.
foo(x) {
  if (x)
    exit(0);
  else
    return 1;
}

Also note that if this exit call is target, it would have been handle by handleCheckerTarget()
correctly and taken.
Overall, if we backtrack to main() and still do not need to take it, then we do not tak the exit(),
otherwise we have to take it and have to take its caller, too (in order to maintain the effect of "exit"). */
void IntraSlicer::handleProcessExitCall(DynInstr *dynInstr) {
  if (DBG)
    stat->printDynInstr(dynInstr, "handleProcessExitCall");
  DynProcessExitCallInstr *exitCall = (DynProcessExitCallInstr *)dynInstr;

  // Start the recursive loop.
  DynCallInstr *caller = exitCall->getCaller();
  DynInstr *dyn = dynInstr;
  bool reason1 = false;
  bool reason2 = false;
  bool remove = false;
  while (true) {
    reason1 = mayCallEvent(caller);
    reason2 = mayWriteFunc(dyn, caller);
    SERRS << "IntraSlicer::handleProcessExitCall reason1 " << reason1
      << ", reason2 " << reason2 << ".\n";
    if (reason1 && reason2) {
      // We have to take the exit() anyway (reason, the foo() above) if we hit a must-not-remove function, in order to ensure the effect of the path: "exit".
      slice.add(dynInstr, TakenFlags::INTRA_EXIT_BOTH); 
      remove = false;
      break;
    } else if (reason1 && !reason2) {
      slice.add(dynInstr, TakenFlags::INTRA_EXIT_CALL_EVENT);
      remove = false;
      break;
    } else if (!reason1 && reason2) {
      slice.add(dynInstr, TakenFlags::INTRA_EXIT_WRITE_FUNC);
      remove = false;
      break;
    } else {
      // DBG.
      stat->printDynInstr(dyn, "handleProcessExitCall dyn");
      if (caller)
        stat->printDynInstr(caller, "handleProcessExitCall caller");
      else
        fprintf(stderr, "handleProcessExitCall caller should have reached main().\n");
      
      // Recursively look back to caller (assign dyn as the 'callee').
      remove = true;
      if (!caller)    // If have reached main(), break, terminate the recursion.
        break;
      dyn = (DynInstr *)caller;
      caller = caller->getCaller();        
    }
  };
  
  if (remove)
    removeRange(dynInstr, caller);
}

void IntraSlicer::handleNonMem(DynInstr *dynInstr) {
  BEGINTIME(stat->intraNonMemSt);
  if (regOverWritten(dynInstr)) {
    delRegOverWritten(dynInstr);
    live.addUsedRegs(dynInstr);
    slice.add(dynInstr, TakenFlags::INTRA_NON_MEM);
  }
  ENDTIME(stat->intraNonMemTime, stat->intraNonMemSt, stat->intraNonMemEnd);
}

void IntraSlicer::handleMem(DynInstr *dynInstr) {
  BEGINTIME(stat->intraMemSt);
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
    DynOprd storePtrOprd(storeInstr, instr->getOperand(1), 1);
    if (storeInstr->isInterThreadTarget()) {
      live.addReg(&storePtrOprd);
      slice.add(storeInstr, TakenFlags::INTER_STORE_TGT);
    }

    // Handle real load instructions in live set.
    DenseSet<DynInstr *> loadInstrs = live.getAllLoadInstrs();
    DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());
    bool alreadyAlias = false; // An optimization flag to skip unnecessary may alias queries.
    for (; itr != loadInstrs.end(); ++itr) {
      DynMemInstr *loadInstr = (DynMemInstr*)(*itr);
      Instruction *staticLoadInstr = idMgr->getOrigInstr((DynInstr *)loadInstr);
      DynOprd loadPtrOprd(loadInstr, staticLoadInstr->getOperand(0), 0);
      if (mustAlias(&loadPtrOprd, &storePtrOprd)) {
        if (loadInstr->isAddrSymbolic() || storeInstr->isAddrSymbolic()) {
          addMemAddrEqConstr(loadInstr, storeInstr);
        }
        live.delLoadMem(loadInstr);
        takeStore(dynInstr, TakenFlags::INTRA_STORE_OW);
        alreadyAlias = true;
        /* Need think: could we jump out of this loop once hit a must-alias?
          Or do we have to look at all load instrs to delete all must-alias load instrs? */
      } else if (!alreadyAlias) {
        /* Optimization: we only need to hit one may alias in order to take this store,
          then we can save lots of may alias queries. */
        if (aliasMgr->mayAlias(&loadPtrOprd, &storePtrOprd)) {
          takeStore(dynInstr, TakenFlags::INTRA_STORE_ALIAS);
          alreadyAlias = true;
        }
      }
    }

    // TODO: IS THIS PRECISE? WHEN DO WE DELETE "EXT LOAD" INSTR FROM LIVE SET?

    // Handle external calls which have semantic "load" in live set.
    if (!alreadyAlias && ((aliasMgr->getPointTee(&storePtrOprd) & live.getExtCallLoadMem()) != bddfalse))
      takeStore(dynInstr, TakenFlags::INTRA_STORE_ALIAS_EXT_CALL);
  }

  ENDTIME(stat->intraMemTime, stat->intraMemSt, stat->intraMemEnd);
}

bool IntraSlicer::mustAlias(DynOprd *oprd1, DynOprd *oprd2) {
  const Value *v1 = oprd1->getStaticValue();
  const Value *v2 = oprd2->getStaticValue();
  if (v1 == v2) {
    if (Util::isConstant(v1) || isa<AllocaInst>(v1))
      return true;
  }
  if (Util::isErrnoAddr(v1) && Util::isErrnoAddr(v2))
    return true;
  return false;
}

DynInstr *IntraSlicer::prevDynBrInstr(DynInstr *dynInstr) {
  uchar tid = dynInstr->getTid();
  size_t tmpIndex = curIndex;
  DynInstr *prevInstr = NULL;
  while (tmpIndex != SIZE_T_INVALID) {
    DynInstr *curInstr = trace->at(tmpIndex);
    if (curInstr->getTid() == tid && Util::isBr(idMgr->getOrigInstr(curInstr))) {
      prevInstr = curInstr;
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

  if (Util::getFunction(prevInstr) != Util::getFunction(postInstr)) {
    stat->printDynInstr(dynPrevInstr, "IntraSlicer::postDominate PREV");
    stat->printDynInstr(dynPostInstr, "IntraSlicer::postDominate POST");
    fprintf(stderr, "Please examine the trace to make sure whether prev and \
      post instructions are within the same function\n");
    dump("IntraSlicer::postDominate function mismatch", dynPrevInstr->getIndex());
    exit(1);
  }

  return result;
}

void IntraSlicer::dump(const char *tag, size_t startIndex) {
  assert(trace);
  errs() << BAN;
  for (size_t i = 0; i < trace->size(); i++) {
    //if (i < startIndex)
      //continue;
    stat->printDynInstr(trace->at(i), tag);
    ctxMgr->printCallStack(trace->at(i));  // Print ctx of each instr so we can easily see which one is wrong.
  }
  errs() << BAN;
}

void IntraSlicer::removeRange(DynInstr *dynInstr, DynCallInstr *caller) {
  if (!caller) {    // If caller is NULL, means we have hit the main() function, done.
    fprintf(stderr, "IntraSlicer::removeRange() should have reached the main() function.\n");
    curIndex = -1;
    if (DBG)
      stat->printDynInstr(dynInstr, "IntraSlicer::removeRange dynInstr reaches main()");
    return;
  }
  /* Directly sets the current slicing index to be the previous one of the call instr. */
  curIndex = caller->getIndex() - 1;
  assert(!empty());
  if (DBG) {
    stat->printDynInstr(dynInstr, "IntraSlicer::removeRange dynInstr");
    stat->printDynInstr(caller, "IntraSlicer::removeRange caller");
  }
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

void IntraSlicer::calStat(set<size_t> &rmBrs, set<size_t> &rmCalls) {
  std::string checkTag = LLVM_CHECK_TAG;
  checkTag += "IntraSlicer::calStat TAKEN";
  size_t numExedInstrs = trace->size();
  size_t numTakenInstrs = slice.size();
  size_t numExedBrs = 0;
  size_t numExedSymBrs = 0;
  size_t numTakenBrs = 0;
  size_t numTakenSymBrs = 0;
  size_t numExedExtCalls = 0;
  size_t numTakenExtCalls = 0;
  
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

    // Handle executed external calls.
    if (Util::isCall(instr) && !funcSumm->isInternalCall(dynInstr))
      numExedExtCalls++;

    // Handle not-taken bi-cond branches, pass to argument.
    if (!dynInstr->isTaken() && Util::isBr(instr) && !Util::isUniCondBr(instr)) {
      rmBrs.insert(dynInstr->getIndex());
      if (DBG)
        stat->printDynInstr(dynInstr, "calStat rmBrs");
    }

    // Handle not-taken external calls.
    if (!dynInstr->isTaken() && Util::isCall(instr) && !funcSumm->isInternalCall(dynInstr)) {
      rmCalls.insert(dynInstr->getIndex());
      if (DBG)
        stat->printDynInstr(dynInstr, "calStat rmCalls");
    }

	/* TODO: actually, in KLEE, forking states are not restricted to branch instructions,
		it could be load/store, malloc()/free(), or even calls to function pointers (with symbolic addresses).
		But currently we only consider removing branches in internal and external calls, we are conservative
		but still correct and sound. Need to think of implementing removing any instructions 
		which forking states in the future.
	*/

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

      // Handle taken external calls.
      if (Util::isCall(instr) && !funcSumm->isInternalCall(dynInstr))
        numTakenExtCalls++;

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
    << "; StaticExed/Static Instrs: " << stat->sizeOfExedStaticInstrs() << "/" << stat->sizeOfStaticInstrs()
    << ";  numTakenExtCalls/numExedExtCalls: " << numTakenExtCalls << "/" << numExedExtCalls
    << ";\n\n\n";

  if (DBG)
    stat->printExternalCalls();
}

