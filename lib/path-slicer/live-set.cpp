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

void LiveSet::init(AliasMgr *aliasMgr, InstrIdMgr *idMgr) {
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
}

size_t LiveSet::virtRegsSize() {
  return virtRegs.size();
}

void LiveSet::clear() {
  virtRegs.clear();
  loadInstrs.clear();
}

void LiveSet::addReg(CallCtx *ctx, const Value *v) {
  if (!Util::isConstant(v)) { // Discard it if it is a LLVM Constant.
    SERRS << "LiveSet::addReg <" << (void *)v << ">: " << *v << "\n";
    CtxVPair p = std::make_pair(ctx, v);
    ASSERT(!DS_IN(p, virtRegs));
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
    SERRS << "LiveSet::addReg OK <" << (void *)(dynOprd->getStaticValue())
      << ">: " << *(dynOprd->getStaticValue()) << "\n";
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

void LiveSet::delLoadMem(DynInstr *dynInstr) {
  ASSERT(DS_IN(dynInstr, loadInstrs));
  loadInstrs.erase(dynInstr);
}

DenseSet<DynInstr *> &LiveSet::getAllLoadInstrs() {
  return loadInstrs;
}

const bdd LiveSet::getAllLoadMem() {
  /* TBD: USE SLOW VERSION TEMPORARILLY UNTIL WE FIGURE OUT HOW TO IMPLEMENT
  REF-COUNTED BDD. */
  //return loadInstrsBdd.getBdd();
  bdd retBDD = bddfalse;

  DenseSet<DynInstr *>::iterator itr(loadInstrs.begin());
  for (; itr != loadInstrs.end(); ++itr) {
    DynInstr *loadInstr = *itr;
    Instruction *staticLoadInstr = idMgr->getOrigInstr(loadInstr);
    DynOprd loadPtrOprd(loadInstr, staticLoadInstr->getOperand(0), 0);
    retBDD |= aliasMgr->getPointTee(&loadPtrOprd);
  }

  return retBDD;
}


