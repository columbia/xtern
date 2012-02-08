#ifndef __TERN_PATH_SLICER_EVENT_MANAGER_H
#define __TERN_PATH_SLICER_EVENT_MANAGER_H

#include <set>

#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/BasicInliner.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/ADT/DenseSet.h"

#include "common/util.h"
#include "common/callgraph-fp.h"

namespace tern {
  struct EventMgr: public llvm::CallGraphFP {	
  public:
    static char ID;

  protected:
    typedef llvm::DenseMap<const llvm::Instruction *, FuncList> SiteFuncMapping;
    typedef llvm::DenseMap<const llvm::Function *, InstList> FuncSiteMapping;
		
    llvm::DenseMap<llvm::Function *, llvm::Function *> parent; // Used in DFS
    llvm::DenseSet<llvm::Function *> visited;
    DenseMap<BasicBlock *, bool> bbVisited;
    std::vector<llvm::Function *> sync_funcs;

    bool is_exit_block(llvm::BasicBlock *bb);
    void DFS(llvm::Function *f);
    void DFS(llvm::BasicBlock *x, llvm::BasicBlock *sink);
    void traverse_call_graph(llvm::Module &M);
    void print_call_chain(llvm::Function *f);
    void print_calling_functions(llvm::Function *f);
    void stats(const llvm::Module &M) const;
    void setupEvents(llvm::Module &M);

  public:
    EventMgr(): llvm::CallGraphFP(&ID) {}
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const;
    bool mayCallEvent(llvm::Function *f);
    bool eventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr);
    bool is_sync_function(llvm::Function *f);
    void output(const llvm::Module &M) const;
    virtual void *getAdjustedAnalysisPointer(const llvm::PassInfo *PI) {   
      if (PI->isPassID(&llvm::CallGraph::ID))
        return (llvm::CallGraph*)this;
      return this;
    }
  };
}

#endif
