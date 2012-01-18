#include "oprd-summ.h"
using namespace tern;
char tern::OprdSumm::ID = 0;

using namespace llvm;

OprdSumm::OprdSumm(): ModulePass(&ID) {

}

OprdSumm::~OprdSumm() {

}

void OprdSumm::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

bool OprdSumm::runOnModule(Module &M) {
  // TBD.
  return false;
}

void OprdSumm::initStat(Stat *stat) {
  this->stat = stat;
}

std::set<llvm::Instruction *> *OprdSumm::getStoreSummBetween(
      DynInstr *prevInstr, DynInstr *postInstr, bdd &bddResults) {
  return NULL;
}

std::set<llvm::Instruction *> *OprdSumm::getStoreSummInFunc(
      DynCallInstr *callInstr, bdd &bddResults) {
  return NULL;
}


