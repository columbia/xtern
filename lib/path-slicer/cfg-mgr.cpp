
#include "cfg-mgr.h"
#include "util.h"
using namespace tern;
char tern::CfgMgr::ID = 0;

using namespace llvm;

CfgMgr::CfgMgr(): ModulePass(&ID) {
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
  return false;
}

bool CfgMgr::postDominate(Instruction *prevInstr, Instruction *postInstr) {
  bool result = false;
  
  // Query cache.
  if (postDomCache.in(0, (long)postInstr, 0, (long)prevInstr, result))
    return result;

  // Query PostDominatorTree.
  Function *f = Util::getFunction(prevInstr);
  assert(f == Util::getFunction(postInstr));
  PostDominatorTree &PDT = getAnalysis<PostDominatorTree>(*f);
  BasicBlock *prevBB = Util::getBasicBlock(prevInstr);
  BasicBlock *postBB = Util::getBasicBlock(postInstr);
  result = PDT.dominates(postBB, prevBB);

  // Update cache.
  postDomCache.add(0, (long)postInstr, 0, (long)prevInstr, result);
  
  return result;
}

