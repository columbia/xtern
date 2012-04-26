#include "live-set.h"
#include "type-defs.h"
#include "macros.h"
#include "util.h"
using namespace tern;

LiveSet::LiveSet() {
  clear();
}

LiveSet::~LiveSet() {

}

void LiveSet::init(AliasMgr *aliasMgr, InstrIdMgr *idMgr, Stat *stat, FuncSumm *funcSumm) {
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
  this->stat = stat;
  this->funcSumm = funcSumm;
}

size_t LiveSet::virtRegsSize() {
  return virtRegs.size();
}

size_t LiveSet::loadInstrsSize() {
  return loadInstrs.size();
}

void LiveSet::clear() {
  virtRegs.clear();
  loadInstrs.clear();
  extCallLoadInstrs.clear();
  allLoadMem = bddfalse;
  extCallLoadMem = bddfalse;
}

void LiveSet::addReg(CallCtx *ctx, Value *v) {
  if (!Util::isConstant(v)) { // Discard it if it is a LLVM Constant.
    //SERRS << "LiveSet::addReg <" << (void *)v << ">: " << *v << "\n";
    CtxVPair p = std::make_pair(ctx, v);
    //ASSERT(!DS_IN(p, virtRegs));
    virtRegs.insert(p);
  }
}

void LiveSet::addReg(DynOprd *dynOprd) {
  if (!Util::isConstant(dynOprd)) { // Discard it if it is a LLVM Constant.
    CtxVPair p = std::make_pair(dynOprd->getDynInstr()->getCallingCtx(), 
      dynOprd->getStaticValue());
    /*if (DS_IN(p, virtRegs)) {
      errs() << "LiveSet::addReg assert failure<" << (void *)(dynOprd->getStaticValue())
      << ">: " << *(dynOprd->getStaticValue()) << "\n";
      CtxVDenseSet::iterator itr(virtRegs.begin());
      for (; itr != virtRegs.end(); ++itr) {
        CtxVPair p = *itr;
        fprintf(stderr, "CtxV in virtRegs: <%p> <%p>\n", (void *)p.first, (void *)p.second);
      }
      assert(false);
    }*/
    /*SERRS << "LiveSet::addReg OK <" << (void *)(dynOprd->getStaticValue())
      << ">: " << *(dynOprd->getStaticValue()) << "\n";*/
    virtRegs.insert(p);
  }
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
  // Checke whether we need to strip cast for function arguments of external calls.
  bool oprdStripCast = false;
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isCall(instr) && !funcSumm->isInternalCall(dynInstr))
    oprdStripCast = true;
  
  // TBD: DO WE NEED TO CONSIDER SIM CALL CTX HERE?
  CallCtx *intCtx = dynInstr->getCallingCtx();
  if (Util::isCall(instr)) {
    CallSite cs  = CallSite(cast<CallInst>(instr));
    // TBD: Some real function call may be wrapped by bitcast? YES WE SHOULD. 
    // REFER TO EXECUTOR.CPP IN KLEE.
    Function *f = cs.getCalledFunction();   
    if (!f) {
      Value *calledV = cs.getCalledValue();
      /* If the called value is not a constant (i.e., a function pointer that can
      have multiple choices), we have to add it to virtual reg. */
      if (!isa<Constant>(calledV)) {    
        errs() << "LiveSet::addUsedRegs called function pointer: ";
        calledV->dump();  // Debugging.
        addReg(intCtx, calledV);
      }
    } else {
      for (CallSite::arg_iterator ci = cs.arg_begin(), ce = cs.arg_end(); ci != ce; ++ci) {
        Value *arg = oprdStripCast?Util::stripCast(*ci):(*ci);
        addReg(intCtx, arg);
      }
    }
  } else {
    for (unsigned i = 0; i < instr->getNumOperands(); i++) {
      addReg(intCtx, instr->getOperand(i));
    }
  }
}

void LiveSet::addLoadMem(DynInstr *dynInstr) {
  ASSERT(!DS_IN(dynInstr, loadInstrs));
  loadInstrs.insert(dynInstr);
  allLoadMem = extCallLoadMem = bddfalse;
}

void LiveSet::addExtCallLoadMem(DynInstr *dynInstr) {
  ASSERT(!DS_IN(dynInstr, extCallLoadInstrs));
  extCallLoadInstrs.insert(dynInstr);  
  allLoadMem = extCallLoadMem = bddfalse;
}

void LiveSet::delLoadMem(DynInstr *dynInstr) {
  ASSERT(DS_IN(dynInstr, loadInstrs));
  loadInstrs.erase(dynInstr);
  allLoadMem = extCallLoadMem = bddfalse;
}

DenseSet<DynInstr *> &LiveSet::getAllLoadInstrs() {
  return loadInstrs;
}

  /* TBD: USE SLOW VERSION TEMPORARILLY UNTIL WE FIGURE OUT HOW TO IMPLEMENT
  REF-COUNTED BDD. */
bdd LiveSet::getAllLoadMem() {
  BEGINTIME(stat->intraLiveLoadMemSt);
  DenseSet<std::pair<void *, void *> > curLoadMemCache; // High level cache.
  bdd retBDD = bddfalse;
  DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());

  // fast path.
  if (allLoadMem != bddfalse) {
    retBDD = allLoadMem;
    goto finish;
  }

  // First, get the load bdd from all real load instructions.
  for (; itr != loadInstrs.end(); ++itr) {
    DynInstr *loadInstr = *itr;
    Instruction *staticLoadInstr = idMgr->getOrigInstr(loadInstr);
    Value *v = staticLoadInstr->getOperand(0);
    PtrPair p = std::make_pair((void *)loadInstr->getCallingCtx(), (void *)v);
    if (DM_IN(p, curLoadMemCache))
      continue;
    DynOprd loadPtrOprd(loadInstr, v, 0);
    if (DBG)
      stat->printDynInstr(loadInstr, "LiveSet::getAllLoadMem RealLoad");
    retBDD |= aliasMgr->getPointTee(&loadPtrOprd);
    curLoadMemCache.insert(p);
  }

  // Second, get the load bdd from all load arguments of external calls.
  retBDD |= getExtCallLoadMem();

finish:
  ENDTIME(stat->intraLiveLoadMemTime, stat->intraLiveLoadMemSt, stat->intraLiveLoadMemEnd);
  allLoadMem = retBDD;
  return retBDD;
}

bdd LiveSet::getExtCallLoadMem() {
  DenseSet<std::pair<void *, void *> > curLoadMemCache; // High level cache.
  bdd retBDD = bddfalse;
  DenseSet<DynInstr *>::iterator itrCall(extCallLoadInstrs.begin());

  // fast path.
  if (extCallLoadMem != bddfalse) {
    retBDD = extCallLoadMem;
    goto finish;
  }

  // slow path.
  for (; itrCall != extCallLoadInstrs.end(); ++itrCall) {
    DynInstr *extCallInstr = *itrCall;
    Instruction *staticCall = idMgr->getOrigInstr(extCallInstr);
    if (DBG)
      stat->printDynInstr(extCallInstr, "LiveSet::getExtCallLoadMem ExtCall");
    assert(isa<CallInst>(staticCall));
    CallSite cs  = CallSite(cast<CallInst>(staticCall));
    unsigned argOffset = 0;
    for (CallSite::arg_iterator ci = cs.arg_begin(), ce = cs.arg_end(); ci != ce; ++ci, ++argOffset) {
      BEGINTIME(stat->getExtCallMemSt);
      bool isExtLoad = funcSumm->isExtFuncSummLoad(staticCall, argOffset);
      ENDTIME(stat->getExtCallMemTime, stat->getExtCallMemSt, stat->getExtCallMemEnd);
      if (isExtLoad) {
        Value *arg = Util::stripCast(*ci);
        PtrPair p = std::make_pair((void *)extCallInstr->getCallingCtx(), (void *)arg);
        if (DM_IN(p, curLoadMemCache))
          continue;
        retBDD |= aliasMgr->getPointTee(extCallInstr, arg);
        curLoadMemCache.insert(p);
      }
    }
  }

finish:
  extCallLoadMem = retBDD;
  return retBDD;
}


