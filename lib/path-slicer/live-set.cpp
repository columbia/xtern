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
  // TBD: DO WE NEED TO CONSIDER SIM CALL CTX HERE?
  CallCtx *intCtx = dynInstr->getCallingCtx();
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isCall(instr)) {
    CallSite cs  = CallSite(cast<CallInst>(instr));
    // TBD: Some real function call may be wrapped by bitcast? YES WE SHOULD. 
    // REFER TO EXECUTOR.CPP IN KLEE.
    Function *f = cs.getCalledFunction();   
    if (!f) {
      Value *calledV = cs.getCalledValue();
      calledV->dump();  // Debugging.
      assert(calledV);
      addReg(intCtx, calledV);
    } else {
      for (CallSite::arg_iterator ci = cs.arg_begin(), ce = cs.arg_end(); ci != ce; ++ci)
        addReg(intCtx, *ci);
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
}

void LiveSet::addExtCallLoadMem(DynInstr *dynInstr) {
  ASSERT(!DS_IN(dynInstr, extCallLoadInstrs));
  extCallLoadInstrs.insert(dynInstr);  
}

void LiveSet::delLoadMem(DynInstr *dynInstr) {
  ASSERT(DS_IN(dynInstr, loadInstrs));
  loadInstrs.erase(dynInstr);
}

DenseSet<DynInstr *> &LiveSet::getAllLoadInstrs() {
  return loadInstrs;
}

  /* TBD: USE SLOW VERSION TEMPORARILLY UNTIL WE FIGURE OUT HOW TO IMPLEMENT
  REF-COUNTED BDD. */
bdd LiveSet::getAllLoadMem() {
  bdd retBDD = bddfalse;

  // First, get the load bdd from all real load instructions.
  DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());
  for (; itr != loadInstrs.end(); ++itr) {
    DynInstr *loadInstr = *itr;
    Instruction *staticLoadInstr = idMgr->getOrigInstr(loadInstr);
    DynOprd loadPtrOprd(loadInstr, staticLoadInstr->getOperand(0), 0);
    if (DBG)
      stat->printDynInstr(loadInstr, "LiveSet::getAllLoadMem RealLoad");
    retBDD |= aliasMgr->getPointTee(&loadPtrOprd);
  }

  // Second, get the load bdd from all load arguments of external calls.
  retBDD |= getExtCallLoadMem();
  return retBDD;
}

bdd LiveSet::getExtCallLoadMem() {
  bdd retBDD = bddfalse;
  DenseSet<DynInstr *>::iterator itrCall(extCallLoadInstrs.begin());
  for (; itrCall != extCallLoadInstrs.end(); ++itrCall) {
    DynInstr *extCallInstr = *itrCall;
    Instruction *staticCall = idMgr->getOrigInstr(extCallInstr);
    if (DBG)
      stat->printDynInstr(extCallInstr, "LiveSet::getExtCallLoadMem ExtCall");
    assert(isa<CallInst>(staticCall));
    CallSite cs  = CallSite(cast<CallInst>(staticCall));
    unsigned argOffset = 0;
    for (CallSite::arg_iterator ci = cs.arg_begin(), ce = cs.arg_end(); ci != ce; ++ci, ++argOffset) {
      if (funcSumm->isExtFuncSummLoad(staticCall, argOffset)) {
        Value *arg = Util::stripCast(*ci);
        retBDD |= aliasMgr->getPointTee(extCallInstr, arg);
      }
    }
  }
  return retBDD;
}


