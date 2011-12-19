#ifndef __TERN_PATH_SLICER_CFG_MGR_H
#define __TERN_PATH_SLICER_CFG_MGR_H

#include "llvm/Instruction.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"

#include "type-defs.h"
#include "cache-util.h"

namespace tern {
  class CfgMgr: public llvm::ModulePass {
  private:
    static char ID;
    /* Cache the nearest postdominators of a given instruction. */
    llvm::DenseMap<const llvm::Instruction *, llvm::Instruction *> neareastPostDomInstr;

    /* Store the info that whether for two instructions, the latter one postdominates the prev one. */
    CacheUtil postDomCache;

  protected:

  public:
    CfgMgr();
    ~CfgMgr();

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);    
    bool postDominate(llvm::Instruction *prevInstr, llvm::Instruction *postInstr);    
  };
}

#endif

