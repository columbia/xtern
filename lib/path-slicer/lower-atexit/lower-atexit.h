#ifndef __TERN_PATH_SLICER_LOWER_ATEXIT_PASS_H
#define __TERN_PATH_SLICER_LOWER_ATEXIT_PASS_H

#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"

#include <vector>

namespace tern {
  struct LowerAtExitPass: public llvm::ModulePass {
  private:
    std::vector<llvm::Function *> atExitFuncs;
    std::vector<llvm::Instruction *> rmAtExitCalls;
    
  public:
    static char ID;
    LowerAtExitPass();
    virtual ~LowerAtExitPass();
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const;
    
    bool collectAtExitFunctions(llvm::Module &M);
    void deleteAtExitCalls(llvm::Module &M);
    void insertCallsBeforeExit(llvm::Module &M);
  };
}

#endif

