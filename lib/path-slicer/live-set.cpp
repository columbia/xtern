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

void LiveSet::init(AliasMgr *aliasMgr, InstrIdMgr *idMgr, Stat *stat, FuncSumm *funcSumm, CallStackMgr *ctxMgr) {
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
  this->stat = stat;
  this->funcSumm = funcSumm;
  this->ctxMgr = ctxMgr;
}

size_t LiveSet::virtRegsSize() {
  return virtRegs.size();
}

size_t LiveSet::loadInstrsSize() {
  return loadInstrs.size();
}

void LiveSet::clear() {
  virtRegs.clear();
  phiVirtRegs.clear();
  loadErrnoInstr = NULL;
  loadInstrs.clear();
  extCallLoadInstrs.clear();
  allLoadMem = bddfalse;
  extCallLoadMem = bddfalse;
}

void LiveSet::addReg(CallCtx *ctx, Value *v) {
  if (!Util::isConstant(v)) { //Discard it if it is a LLVM Constant
    if (DBG)
      errs() << "LiveSet::addReg <" << (void *)v << ">: " << stat->printValue(v) << "\n\n";
    CtxVPair p = std::make_pair(ctx, v);
    virtRegs.insert(p);
    if (Util::isPHI(v))
      phiVirtRegs.insert(p);
  }
}

void LiveSet::addReg(DynOprd *dynOprd) {
  Value *v = dynOprd->getStaticValue();
  if (!Util::isConstant(v)) { //Discard it if it is a LLVM Constant
    CtxVPair p = std::make_pair(dynOprd->getDynInstr()->getCallingCtx(), v);
    virtRegs.insert(p);
    if (Util::isPHI(v))
      phiVirtRegs.insert(p);
  }
}

void LiveSet::delReg(DynOprd *dynOprd) {
  Value *v = dynOprd->getStaticValue();
  CtxVPair p = std::make_pair(dynOprd->getDynInstr()->getCallingCtx(), v);
  ASSERT(DS_IN(p, virtRegs));
  virtRegs.erase(p);
  if (Util::isPHI(v))
    phiVirtRegs.erase(p);
}

CtxVDenseSet &LiveSet::getAllRegs() {
  return virtRegs;
}

bool LiveSet::regIn(DynOprd *dynOprd) {
  CtxVPair p = std::make_pair(
    dynOprd->getDynInstr()->getCallingCtx(), 
    dynOprd->getStaticValue());
  
  if (DBG) {
    Instruction *instr = idMgr->getOrigInstr(dynOprd->getDynInstr());
    if (instr && Util::isCall(instr)) {
      printVirtReg(p, "LiveSet::regIn the checked register");
      printVirtRegs("LiveSet::regIn the virt registers");
    }
  }

  return (DS_IN(p, virtRegs));
}

void LiveSet::addInnerUsedRegs(CallCtx *intCtx, User *user) {
  unsigned numOperands = user->getNumOperands();
  for(unsigned i = 0; i < numOperands; i++) {
    Value *oprd = user->getOperand(i);
    //if (DBG)
      //errs() << "LiveSet::addInnerUsedRegs handles oprd " << stat->printValue(oprd) << "\n";
    ConstantExpr *opInner = dyn_cast<ConstantExpr>(oprd);
    if (opInner)
      addInnerUsedRegs(intCtx, opInner);
    else // Only need to add this reg when it is not a LLVM Constant.
      addReg(intCtx, oprd);
  }
}

void LiveSet::addUsedRegs(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  CallCtx *intCtx = dynInstr->getCallingCtx();
  // This also handles function call arguments.
  for (unsigned i = 0; i < instr->getNumOperands(); i++) {
    Value *oprd = instr->getOperand(i);
    ConstantExpr *opInner = dyn_cast<ConstantExpr>(oprd);
    if(opInner) {
      /*if (DBG)
        errs() << "LiveSet::addUsedRegs handles nested instruction " << stat->printInstr(instr) << "\n"
          << "Oprd " << stat->printValue(oprd) << "\n";*/
      addInnerUsedRegs(intCtx, opInner);
    } else // Only need to add this reg when it is not a LLVM Constant.
      addReg(intCtx, oprd);
  }
}

void LiveSet::cleanLoadMemBdd() {
  allLoadMem = extCallLoadMem = bddfalse;
}

void LiveSet::addLoadMem(DynInstr *dynInstr) {
  ASSERT(!DS_IN(dynInstr, loadInstrs));
  if (isLoadErrnoInstr(dynInstr)) {
    loadErrnoInstr = dynInstr;
    if (DBG)
      stat->printDynInstr(loadErrnoInstr, "LiveSet::addLoadMem update loadErrnoInstr");
  }
  loadInstrs.insert(dynInstr);
  cleanLoadMemBdd();
}

void LiveSet::addExtCallLoadMem(DynInstr *dynInstr) {
  ASSERT(!DS_IN(dynInstr, extCallLoadInstrs));
  extCallLoadInstrs.insert(dynInstr);  
  cleanLoadMemBdd();
}

void LiveSet::delLoadMem(DynInstr *dynInstr) {
  ASSERT(DS_IN(dynInstr, loadInstrs));
  loadInstrs.erase(dynInstr);
  if (loadErrnoInstr == dynInstr) // If the current removed dyn instr is load errno instr, set it to be NULL too.
    loadErrnoInstr = NULL;
  cleanLoadMemBdd();
}

void LiveSet::delLoadErrnoInstr() {
  ASSERT(loadErrnoInstr);
  if (DBG)
    stat->printDynInstr(loadErrnoInstr, "LiveSet::delLoadErrnoInstr");
  delLoadMem(loadErrnoInstr);
  cleanLoadMemBdd();
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
    ASSERT(isa<CallInst>(staticCall));
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

bool LiveSet::phiDefBetween(CallCtx *ctx, InstrDenseSet *phiSet) {
  InstrDenseSet::iterator itr(phiSet->begin());
  for (; itr != phiSet->end(); ++itr) {
    CtxVPair p = std::make_pair(ctx, (Value *)(*itr));
    if (phiVirtRegs.count(p))
      return true;
  }
  return false;
}

void LiveSet::printVirtRegs(const char *tag) {
  CtxVDenseSet::iterator itr(virtRegs.begin());
  errs() << BAN;
  for (; itr != virtRegs.end(); ++itr)
    printVirtReg(*itr, tag);
}

void LiveSet::printVirtReg(CtxVPair &virtReg, const char *tag) {
  //assert(virtReg.second != NULL);
  CallCtx *ctx = virtReg.first;
  Value *v = virtReg.second;
  errs() << "Liveset::printVirtReg: tag: " << tag << ": Ctx: " << ctx << ": ";
  ctxMgr->printCallStack(ctx);
  errs() << "Value: " << v << ": ";
  errs() << stat->printValue(v) << "\n";
  errs() << BAN;
}

bool LiveSet::isLoadErrnoInstr(DynInstr *dynInstr) {
   Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isLoad(instr))
    return Util::isErrnoAddr(instr->getOperand(0));
  else
    return false;
}

DynInstr *LiveSet::getLoadErrnoInstr() {
  return loadErrnoInstr;
}

