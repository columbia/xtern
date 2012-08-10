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

#include "common/callgraph-fp.h"
#include "common/typedefs.h"
#include "klee/BasicCheckers.h"

namespace tern {
  struct EventMgr: public llvm::ModulePass {	
  public:
    llvm::Module *module;
    static char ID;

  protected:
    klee::Checker *checker;
    llvm::CallGraphFP *CG;
	
    typedef llvm::DenseMap<const llvm::Instruction *, FuncList> SiteFuncMapping;
    typedef llvm::DenseMap<const llvm::Function *, InstList> FuncSiteMapping;		

    /* Internal functions which may call events (events calling with std* are ignored). */
    llvm::DenseSet<llvm::Function *> mayCallEventFuncs; 

    /* Event functions such as fopen, fclose(). These functions must be external. */
    llvm::DenseSet<llvm::Function *> eventFuncs; 

    /* Call instructions which call non-ignored events. */
    llvm::DenseSet<llvm::Instruction *> eventCallSites; 

    /* Call Instructions which may call events in eventCallSites (but excluding eventCallSites).
    E.g., call foo(), and foo() calls fopen(), so "call foo()" is in. */
    llvm::DenseSet<llvm::Instruction *> mayCallEventInstrs; 
	
    DenseMap<BasicBlock *, bool> bbVisited;

    bool is_exit_block(llvm::BasicBlock *bb);
    void DFSCollectMayCallEventFuncs(llvm::Function *f, bool isTop);
    void DFS(llvm::BasicBlock *x, llvm::BasicBlock *sink);
    void traverse_call_graph(llvm::Module &M);
    void setupEvents(llvm::Module &M);
    void collectStaticEventCalls(llvm::Function *event);
    static bool isIgnoredEventCall(llvm::Instruction *call, llvm::Function *event);
    static bool isStdErrOrOut(llvm::Value *v);
    void printDBG();

    /* Whether an instruction may call events. It can be a direct call to an event, or can be calling an internal function which may contain event calls. */
    bool mayCallEvent(llvm::Instruction *instr);

  public:
    EventMgr();
    ~EventMgr();
    void initCallGraph(llvm::CallGraphFP *CG);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);

    /* Whether an internal function f may call events. The f must be an internal function. */
    bool mayCallEvent(llvm::Function *f);
	
    bool eventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr);
    bool isEventCall(llvm::Instruction *instr);
    void output(const llvm::Module &M) const;
    size_t numEventCallSites();
    void printEventCalls();
  };
}

#endif
