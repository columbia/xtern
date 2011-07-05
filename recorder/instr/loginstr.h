/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGINSTR_H
#define __TERN_RECORDER_LOGINSTR_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/raw_ostream.h"

namespace tern 
{
  struct LogInstr: public llvm::ModulePass 
  {
    static char ID;
    llvm::TargetData *targetData;
    llvm::LLVMContext *context;

    const llvm::PointerType *addrType;
    const llvm::IntegerType *dataType;
    const llvm::Value       *logFunc;

    LogInstr(): ModulePass(&ID) {}
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    void instrFunc(llvm::Function &F);
    bool instrLoad(llvm::LoadInst *load);
    bool instrStore(llvm::StoreInst *store);
    void instrInstruction(llvm::Value *insid, llvm::Value *addr, 
           llvm::Value *data, llvm::Instruction *ins, bool after=true);
    bool instrCall(llvm::CallInst *call);
    bool instrInvoke(llvm::InvokeInst *invoke);
    bool instrBB(llvm::BasicBlock *BB);
  };
}

#endif
