
#include "cfg-mgr.h"
#include "util.h"
using namespace tern;
char tern::CfgMgr::ID = 0;

using namespace llvm;

CfgMgr::CfgMgr(): ModulePass(&ID) {
  postDomCache.clear();
  firstInstr = NULL;
}

CfgMgr::~CfgMgr() {
  
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
  Function *f = M.getFunction("main");
  assert(f);
  firstInstr = &(f->getEntryBlock().front());
  return false;
}

Instruction *CfgMgr::getFirstInstr() {
  assert(firstInstr);
  return firstInstr;
}


bool CfgMgr::postDominate(Instruction *prevInstr, Instruction *postInstr) {
  SERRS << "CfgMgr::postDominate PREV: " << stat->printInstr(prevInstr) << "\n";
  SERRS << "CfgMgr::postDominate POST: " << stat->printInstr(postInstr) << "\n";
  
  bool result = false;
  
  // Query cache.
  if (postDomCache.in(0, (long)postInstr, 0, (long)prevInstr, result))
    return result;

  // Query PostDominatorTree.
  Function *f = Util::getFunction(postInstr);  
  PostDominatorTree &PDT = getAnalysis<PostDominatorTree>(*f);
  BasicBlock *prevBB = Util::getBasicBlock(prevInstr);
  BasicBlock *postBB = Util::getBasicBlock(postInstr);
  result = PDT.dominates(postBB, prevBB);

  // Update cache.
  postDomCache.add(0, (long)postInstr, 0, (long)prevInstr, result);
  
  return result;
}

BasicBlock *CfgMgr::findNearestCommonPostDominator(PostDominatorTree &PDT, 
  BasicBlock *A, BasicBlock *B) {  
  assert(A->getParent() == B->getParent());
  // If B dominates A then B is nearest common dominator.
  if (PDT.dominates(B, A))
    return B;

  // If A dominates B then A is nearest common dominator.
  if (PDT.dominates(A, B))
    return A;

  DomTreeNodeBase<BasicBlock> *NodeA = PDT.getNode(A);
  DomTreeNodeBase<BasicBlock> *NodeB = PDT.getNode(B);

  // Collect NodeA dominators set.
  SmallPtrSet<DomTreeNodeBase<BasicBlock>*, 16> NodeADoms;
  NodeADoms.insert(NodeA);
  DomTreeNodeBase<BasicBlock> *IDomA = NodeA->getIDom();
  while (IDomA) {
    NodeADoms.insert(IDomA);
    IDomA = IDomA->getIDom();
  }

  // Walk NodeB immediate dominators chain and find common dominator node.
  DomTreeNodeBase<BasicBlock> *IDomB = NodeB->getIDom();
  while (IDomB) {
    if (NodeADoms.count(IDomB) != 0)
      return IDomB->getBlock();

    IDomB = IDomB->getIDom();
  }

  return NULL;
}

Instruction *CfgMgr::getStaticPostDom(Instruction *prevInstr) {
  /*static long numPostDomQuery;
  static long numPostDomHit;
  numPostDomQuery++;
  if (numPostDomQuery%10000 == 0)
    fprintf(stderr, "PostDom::findPostDomInst %ld/%ld, time build tree %ld\n",
    numPostDomHit, numPostDomQuery, timeBuildingPostDomTree);*/
  
  if (neareastPostDomInst.count(prevInstr) > 0) {
    //numPostDomHit++;
    return neareastPostDomInst[prevInstr];
  }

  assert(isa<BranchInst>(prevInstr));
  BranchInst *branch = dyn_cast<BranchInst>(prevInstr);
  assert(branch->getNumSuccessors() > 0 && "The branch has no successor.");

  /* Calculate the nearest post dominator of <bb> */
  BasicBlock *postDominatorBB = branch->getSuccessor(0);
  PostDominatorTree &PDT = getAnalysis<PostDominatorTree>(*(postDominatorBB->getParent()));
  for (unsigned i = 1; i < branch->getNumSuccessors(); i++) {
    postDominatorBB = findNearestCommonPostDominator(PDT, postDominatorBB, 
      branch->getSuccessor(i));
    if (!postDominatorBB)
      break;
  }

  if (postDominatorBB)
    neareastPostDomInst[prevInstr] = &(postDominatorBB->front());
  else
    neareastPostDomInst[prevInstr] = NULL;
  return neareastPostDomInst[prevInstr];
}

