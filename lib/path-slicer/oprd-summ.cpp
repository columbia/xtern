#include "util.h"
#include "oprd-summ.h"
using namespace tern;
char tern::OprdSumm::ID = 0;

#include "common/util.h"
#include "common/callgraph-fp.h"
using namespace llvm;

OprdSumm::OprdSumm(): ModulePass(&ID) {
  funcSumm = NULL;
}

OprdSumm::~OprdSumm() {

}

void OprdSumm::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<CallGraphFP>();
  ModulePass::getAnalysisUsage(AU);
}

bool OprdSumm::runOnModule(Module &M) {
  fprintf(stderr, "OprdSumm::runOnModule begin\n");
  collectSummLocal(M);
  collectSummTopDown(M);
  fprintf(stderr, "OprdSumm::runOnModule end\n");
  clean();
  return false;
}

void OprdSumm::clean() {
  fprintf(stderr, "EventMgr::clean\n");
  CallGraphFP &CG = getAnalysis<CallGraphFP>();
  CG.destroy();
  /* We have to destroy the CallGraph here, since when uclibc is linking in, 
  LLVM would remove some functions in original modules and link in ones in 
  uclibc, and the removal would cause crash if we do not free the CallGraph 
  before hand. But there is no problem because callgraph-fp maintains callsites
  independently. */
}


void OprdSumm::init(Stat *stat, FuncSumm *funcSumm,
  AliasMgr *aliasMgr, InstrIdMgr *idMgr) {
  this->stat = stat;
  this->funcSumm = funcSumm;
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
}

void OprdSumm::printSumm(InstrDenseSet &summ, const char *tag) {
  return;
  InstrDenseSet::iterator itr(summ.begin());
  for (; itr != summ.end(); ++itr)
    errs() << stat->printInstr(*itr, tag);
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
  BasicBlock *x = Util::getBasicBlock(idMgr->getOrigInstr(prevBrInstr));
  BasicBlock *sink = Util::getBasicBlock(idMgr->getOrigInstr(postInstr));
  for (succ_iterator it = succ_begin(x); it != succ_end(x); ++it) {
    BasicBlock *y = *it;
    if (y == prevBrInstr->getSuccessorBB())   // We can ignore the executed branch.
      continue;
    DFSBasicBlock(y, sink, summ, Store);
  }

  // Get bdd of these store instructions with the calling context.
  printSumm(summ, "OprdSumm::getStoreSummBetween");
  bddResults = bddfalse;
  InstrDenseSet::iterator itr(summ.begin());
  for (; itr != summ.end(); ++itr) {
    /* The instructions here are already from either normal or max slicing 
    module depending on slicing mode, so this is correct. */
    const Instruction *storeInstr = *itr;
    bddResults |= aliasMgr->getPointTee(prevBrInstr, storeInstr->getOperand(1));
  }
  return NULL;
}

InstrDenseSet *OprdSumm::getStoreSummInFunc(
      DynRetInstr *retInstr, bdd &bddResults) {
  DynCallInstr *callInstr = retInstr->getDynCallInstr();
  Function *calledFunc = callInstr->getCalledFunc();
  assert(calledFunc);
  visitedBB.clear();
  InstrDenseSet *summ = funcStoreSumm[calledFunc];
  assert(summ);

  // Get bdd of these store instructions with the calling context (of the dynamic return instruction).
  printSumm(*summ, "OprdSumm::getStoreSummInFunc");
  bddResults = bddfalse;
  InstrDenseSet::iterator itr(summ->begin());
  for (; itr != summ->end(); ++itr) {
    /* The instructions here are already from either normal or max slicing 
    module depending on slicing mode, so this is correct. */
    const Instruction *storeInstr = *itr;
    bddResults |= aliasMgr->getPointTee(retInstr, storeInstr->getOperand(1));
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
  } else {
    InstrDenseSet *storeSet = bbStoreSumm[x];
    assert(storeSet);
    summ.insert(storeSet->begin(), storeSet->end());
  }
  
  for (succ_iterator it = succ_begin(x); it != succ_end(x); ++it) {
    BasicBlock *y = *it;
    DFSBasicBlock(y, sink, summ, oprdType);
  }
}


void OprdSumm::initAllSumm(llvm::Module &M) {
  for (Module::iterator fi = M.begin(); fi != M.end(); ++fi) {
    funcLoadSumm[fi] = funcStoreSumm[fi] = NULL;
    for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
      bbLoadSumm[bi] = bbStoreSumm[bi] = NULL;
    }
  }
}

void OprdSumm::collectSummLocal(llvm::Module &M) {
  visited.clear();
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
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

void OprdSumm::collectFuncSummLocal(const llvm::Function *f) {
  for (Function::const_iterator bi = f->begin(); bi != f->end(); ++bi) {
    bbLoadSumm[bi] = new InstrDenseSet;
    bbStoreSumm[bi] = new InstrDenseSet;
    for (BasicBlock::const_iterator ii = bi->begin(); ii != bi->end(); ++ii) {
      collectInstrSummLocal(ii);
    }
  } 
}

void OprdSumm::collectInstrSummLocal(const llvm::Instruction *instr) {
  const Function *f = Util::getFunction(instr);
  const BasicBlock *bb = Util::getBasicBlock(instr);
  if (instr->getOpcode() == Instruction::Load) {
    bbLoadSumm[bb]->insert(instr);
    funcLoadSumm[f]->insert(instr);
  } else if (instr->getOpcode() == Instruction::Store) {
    bbStoreSumm[bb]->insert(instr);
    funcStoreSumm[f]->insert(instr);
  }
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

void OprdSumm::DFSTopDown(const llvm::Function *f) {
  CallGraphFP &CG = getAnalysis<CallGraphFP>();
  for (Function::const_iterator b = f->begin(), be = f->end(); b != be; ++b) {
    for (BasicBlock::const_iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
      if (Util::isCall(i) && funcSumm->isInternalCall(i)) {
        vector<Function *> calledFuncs = CG.get_called_functions(i);
        for (size_t j = 0; j < calledFuncs.size(); ++j) {
          const Function *callee = calledFuncs[j];

          // First, do DFS to collect summary.
          if (visited.count(callee) == 0) {
            visited.insert(callee);
            if (funcSumm->isInternalFunction(callee)) {
              DFSTopDown(callee);
            }
          }

          // Add the collected summary to caller.
          addSummToCallerTopDown(callee, f, i);
        }
      }
    }
  }

}

void OprdSumm::addSummToCallerTopDown(const llvm::Function *callee,
  const llvm::Function *caller, const Instruction *callInstr) {
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
  const BasicBlock *bb = Util::getBasicBlock(callInstr);
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

