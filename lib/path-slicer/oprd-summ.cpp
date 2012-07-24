#include "util.h"
#include "oprd-summ.h"
using namespace tern;
char tern::OprdSumm::ID = 0;

#include "common/util.h"
using namespace llvm;

OprdSumm::OprdSumm(): ModulePass(&ID) {
  funcSumm = NULL;
  CG = NULL;
}

OprdSumm::~OprdSumm() {

}

void OprdSumm::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

bool OprdSumm::runOnModule(Module &M) {
  fprintf(stderr, "OprdSumm::runOnModule begin\n");
  collectSummLocal(M);
  collectSummTopDown(M);
  fprintf(stderr, "OprdSumm::runOnModule end\n");
  return false;
}

void OprdSumm::init(Stat *stat, FuncSumm *funcSumm, AliasMgr *aliasMgr,
  InstrIdMgr *idMgr, llvm::CallGraphFP *CG) {
  this->stat = stat;
  this->funcSumm = funcSumm;
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
  this->CG = CG;
}

void OprdSumm::printSumm(InstrDenseSet &summ, const char *tag) {
  if (!DBG)
    return;
  InstrDenseSet::iterator itr(summ.begin());
  for (; itr != summ.end(); ++itr)
    errs() << tag << ": " << stat->printInstr(*itr) << "\n";
}

InstrDenseSet *OprdSumm::getLoadSummBetween(
      DynInstr *prevInstr, DynInstr *postInstr, bdd &bddResults) {
  return NULL;
}

InstrDenseSet *OprdSumm::getStoreSummBetween(
  DynBrInstr *prevBrInstr, DynInstr *postInstr, bdd &bddResults) {
  // Collect static store instructions.
  visitedBB.clear();
  InstrDenseSet summ;
  BEGINTIME(stat->intraBrWrBetGetSummSt);
  BasicBlock *x = Util::getBasicBlock(idMgr->getOrigInstr(prevBrInstr));
  BasicBlock *sink = Util::getBasicBlock(idMgr->getOrigInstr(postInstr));
  for (succ_iterator it = succ_begin(x); it != succ_end(x); ++it) {
    BasicBlock *y = *it;
    if (y == prevBrInstr->getSuccessorBB())   // We can ignore the executed branch.
      continue;
    DFSBasicBlock(y, sink, summ, Store);
  }
  ENDTIME(stat->intraBrWrBetGetSummTime, stat->intraBrWrBetGetSummSt, stat->intraBrWrBetGetSummEnd);

  // Get bdd of these store instructions with the calling context.
  bddResults = bddfalse;
  InstrDenseSet::iterator itr(summ.begin());
  BEGINTIME(stat->intraBrWrBetGetBddSt);
  for (; itr != summ.end(); ++itr) {
    if (Util::isStore(*itr)) {
      /* The instructions here are already from either normal or max slicing 
      module depending on slicing mode, so this is correct. */
      Instruction *storeInstr = *itr;
      SERRS << "\nOprdSumm::getStoreSummBetween Store: " << stat->printInstr(storeInstr) << "\n";
      bddResults |= aliasMgr->getPointTee(prevBrInstr, storeInstr->getOperand(1));
    } else {
      Instruction *instr = *itr;
      assert(Util::isCall(instr));
      CallSite cs  = CallSite(cast<CallInst>(instr));
      unsigned argOffset = 0;
      for (CallSite::arg_iterator ci = cs.arg_begin(), ce = cs.arg_end();
        ci != ce; ++ci, ++argOffset) {
        if (funcSumm->isExtFuncSummStore(instr, argOffset)) {
          Value *arg = Util::stripCast(*ci);
          SERRS << "\nOprdSumm::getStoreSummBetween ExtCall" << stat->printInstr(instr)
            << " argOffset: " << argOffset << "\n";
          bddResults |= aliasMgr->getPointTee(prevBrInstr, arg);
        }
      }
    }
  }
  ENDTIME(stat->intraBrWrBetGetBddTime, stat->intraBrWrBetGetBddSt, stat->intraBrWrBetGetBddEnd);
  return NULL;
}

InstrDenseSet *OprdSumm::getStoreSummInFunc(
  DynInstr *dynInstr, DynCallInstr *caller, bdd &bddResults) {
  Function *calledFunc = NULL;
  if (caller)
    calledFunc = caller->getCalledFunc();
  else
    calledFunc = mainFunc;
  assert(calledFunc);
  visitedBB.clear();
  InstrDenseSet *summ = funcStoreSumm[calledFunc];
  assert(summ);

  // Get bdd of these store instructions with the calling context (of the dynamic return instruction).
  bddResults = bddfalse;
  InstrDenseSet::iterator itr(summ->begin());
  for (; itr != summ->end(); ++itr) {
    if (Util::isStore(*itr)) {
      /* The instructions here are already from either normal or max slicing 
      module depending on slicing mode, so this is correct. */
      Instruction *storeInstr = *itr;
      SERRS << "\nOprdSumm::getStoreSummInFunc: " << stat->printInstr(storeInstr) << "\n";
      bddResults |= aliasMgr->getPointTee(dynInstr, storeInstr->getOperand(1));
    } else {
      Instruction *instr = *itr;
      assert(Util::isCall(instr));
      CallSite cs  = CallSite(cast<CallInst>(instr));
      unsigned argOffset = 0;
      for (CallSite::arg_iterator ci = cs.arg_begin(), ce = cs.arg_end();
        ci != ce; ++ci, ++argOffset) {
        if (funcSumm->isExtFuncSummStore(instr, argOffset)) {
          Value *arg = Util::stripCast(*ci);
          SERRS << "\nOprdSumm::getStoreSummInFunc ExtCall argOffSet[" << argOffset << "]: "
            << stat->printInstr(instr) << "\n";
          bddResults |= aliasMgr->getPointTee(dynInstr, arg);
        }
      }
    }
  }
  return NULL;
}

InstrDenseSet *OprdSumm::getExtCallStoreSumm(DynCallInstr *callInstr,
  bdd &bddResults) {
  bddResults = bddfalse;
  Instruction *instr = idMgr->getOrigInstr((DynInstr *)callInstr);
  assert(isa<CallInst>(instr));
  CallSite cs  = CallSite(cast<CallInst>(instr));
  unsigned argOffset = 0;
  for (CallSite::arg_iterator ci = cs.arg_begin(), ce = cs.arg_end();
    ci != ce; ++ci, ++argOffset) {
    if (funcSumm->isExtFuncSummStore(instr, argOffset)) {
      Value *arg = Util::stripCast(*ci);
      SERRS << "OprdSumm::getExtCallStoreSumm ExtStore arg[" << argOffset << "]: "
        << stat->printInstr(instr) << "n";
      bddResults |= aliasMgr->getPointTee(callInstr, arg);
    }
  }
  return NULL;
}


void OprdSumm::DFSBasicBlock(BasicBlock *x, BasicBlock *sink,
  InstrDenseSet &summ, OprdType oprdType) {
  if (visitedBB.count(x))
    return;
  // Stop at the sink -- the post dominator of the branch
  if (x == sink)
    return;
  visitedBB.insert(x);

  // Collect summary based on oprdType.
  if (oprdType == Load) {
    InstrDenseSet *loadSet = bbLoadSumm[x];
    assert(loadSet);
    summ.insert(loadSet->begin(), loadSet->end());
  } else if (oprdType == Store) {
    InstrDenseSet *storeSet = bbStoreSumm[x];
    assert(storeSet);
    summ.insert(storeSet->begin(), storeSet->end());
  } else {
    assert(oprdType == PHI);
    InstrDenseSet *phiSet = bbPhiDefSumm[x];
    assert(phiSet);
    summ.insert(phiSet->begin(), phiSet->end());
  }
  
  for (succ_iterator it = succ_begin(x); it != succ_end(x); ++it) {
    BasicBlock *y = *it;
    DFSBasicBlock(y, sink, summ, oprdType);
  }
}


void OprdSumm::initAllSumm(llvm::Module &M) {
  for (Module::iterator fi = M.begin(); fi != M.end(); ++fi) {
    funcLoadSumm[fi] = NULL;
    funcStoreSumm[fi] = NULL;
    for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
      bbLoadSumm[bi] = NULL;
      bbStoreSumm[bi] = NULL;
      bbPhiDefSumm[bi] = NULL;
    }
  }
}

void OprdSumm::collectSummLocal(llvm::Module &M) {
  visited.clear();
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {    
    // Record the main function.
    if (f->getNameStr() == "main" || f->getNameStr() == "__user_main")
      mainFunc = f;
    
    if (visited.count(f) == 0) {
      visited.insert(f);
      if (funcSumm->isInternalFunction(f)) {
        funcLoadSumm[f] = new InstrDenseSet;
        funcStoreSumm[f] = new InstrDenseSet;
        collectFuncSummLocal(f);
      }
    }
  }
}

void OprdSumm::collectFuncSummLocal(llvm::Function *f) {
  for (Function::iterator bi = f->begin(); bi != f->end(); ++bi) {
    bbLoadSumm[bi] = new InstrDenseSet;
    bbStoreSumm[bi] = new InstrDenseSet;
    bbPhiDefSumm[bi] = new InstrDenseSet;
    for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
      collectInstrSummLocal(ii);
    }
  } 
}

void OprdSumm::collectInstrSummLocal(llvm::Instruction *instr) {
  Function *f = Util::getFunction(instr);
  BasicBlock *bb = Util::getBasicBlock(instr);
  if (Util::isLoad(instr)) {
    bbLoadSumm[bb]->insert(instr);
    funcLoadSumm[f]->insert(instr);
  } else if (Util::isStore(instr)) {
    bbStoreSumm[bb]->insert(instr);
    funcStoreSumm[f]->insert(instr);
  } else if (Util::isCall(instr)) {
    if (!funcSumm->isInternalCall(instr)) {
      if (funcSumm->extFuncHasLoadSumm(instr)) { // External function load summary.
        SERRS << "collectInstrSummLocal ExtLoad: " << stat->printInstr(instr) << "\n";
        bbLoadSumm[bb]->insert(instr);
        funcLoadSumm[f]->insert(instr);
      }
      if (funcSumm->extFuncHasStoreSumm(instr)) { // External function store summary.
        SERRS << "collectInstrSummLocal ExtStore: " << stat->printInstr(instr) << "\n";
        bbStoreSumm[bb]->insert(instr);
        funcStoreSumm[f]->insert(instr);
      }
    }
  }

  Util::addUsedByPhiNodes(instr, bbPhiDefSumm[bb]);
}

void OprdSumm::collectSummTopDown(llvm::Module &M) {
  visited.clear();
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    if (visited.count(f) == 0) {
      visited.insert(f);
      if (funcSumm->isInternalFunction(f)) {
        DFSTopDown(f);
      }
    }
  }
}

void OprdSumm::DFSTopDown(llvm::Function *f) {
  for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
    for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
      if (Util::isCall(i)) {
        vector<Function *> calledFuncs = CG->get_called_functions(i);
        for (size_t j = 0; j < calledFuncs.size(); ++j) {
          Function *callee = calledFuncs[j];

          // First, do DFS to collect summary.
          if (visited.count(callee) == 0) {
            visited.insert(callee);
            if (funcSumm->isInternalFunction(callee)) {
              DFSTopDown(callee);
            }
          }

          /* Add the collected summary to caller, if the callee is an internal function.
          If it is external, it does not have any summary. */
          if (funcSumm->isInternalFunction(callee))
            addSummToCallerTopDown(callee, f, i);
        }
      }
    }
  }

}

void OprdSumm::addSummToCallerTopDown(llvm::Function *callee,
  llvm::Function *caller, Instruction *callInstr) {
  // Add function load summary from callee to caller.
  if (funcLoadSumm[callee]) {
    if (!funcLoadSumm[caller])
      funcLoadSumm[caller] = new InstrDenseSet;
    addSummTopDown(funcLoadSumm[callee], funcLoadSumm[caller]);
  }

  // Add function store summary from callee to caller.
  if (funcStoreSumm[callee]) {
    if (!funcStoreSumm[caller])
      funcStoreSumm[caller] = new InstrDenseSet;
    addSummTopDown(funcStoreSumm[callee], funcStoreSumm[caller]);
  }

  // Add function load summary from callee to the bb containing the call instruction.
  BasicBlock *bb = Util::getBasicBlock(callInstr);
  if (funcLoadSumm[callee]) {
    if (!bbLoadSumm[bb])
      bbLoadSumm[bb] = new InstrDenseSet;
    addSummTopDown(funcLoadSumm[callee], bbLoadSumm[bb]);
  }

  // Add function store summary from callee to the bb containing the call instruction.
  if (funcStoreSumm[callee]) {
    if (!bbStoreSumm[bb])
      bbStoreSumm[bb] = new InstrDenseSet;
    addSummTopDown(funcStoreSumm[callee], bbStoreSumm[bb]);
  }
}

void OprdSumm::addSummTopDown(InstrDenseSet *calleeSet,
  InstrDenseSet *callerSet) {
  InstrDenseSet::iterator itr(calleeSet->begin());
  for (; itr != calleeSet->end(); ++itr)
    callerSet->insert(*itr);
}

void OprdSumm::getUsedByPhiSummBetween(DynBrInstr *prevBrInstr, DynInstr *postInstr, InstrDenseSet &phiSet) {
  // TBD. traverse the basicblocks and then collect all phinodes from the bbPhiDefSumm.
  visitedBB.clear();
  BasicBlock *x = Util::getBasicBlock(idMgr->getOrigInstr(prevBrInstr));
  BasicBlock *sink = Util::getBasicBlock(idMgr->getOrigInstr(postInstr));
  for (succ_iterator it = succ_begin(x); it != succ_end(x); ++it) {
    BasicBlock *y = *it;
    if (y == prevBrInstr->getSuccessorBB())   // We can ignore the executed branch.
      continue;
    DFSBasicBlock(y, sink, phiSet, PHI);
  }
  printSumm(phiSet, "OprdSumm::getUsedByPhiSummBetween");
}

