
#include "cfg-mgr.h"
using namespace tern;
char tern::CfgMgr::ID = 0;

using namespace llvm;

CfgMgr::CfgMgr(): ModulePass(&ID) {
  neareastPostDomInstr.clear();
  postDomCache.clear();
}

void CfgMgr::initStat(Stat *stat) {
  this->stat = stat;
}

void CfgMgr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<PostDominatorTree>();
  ModulePass::getAnalysisUsage(AU);
}

bool CfgMgr::runOnModule(Module &M) {
  // TBD.
  return false;
}

bool CfgMgr::postDominate(Instruction *prevInstr, Instruction *postInstr) {
  return false;
}

