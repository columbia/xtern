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
    llvm::Value       *logFunc;
    llvm::Value       *logCallFunc;
    llvm::Value       *logRetFunc;

    LogInstr(): ModulePass(&ID) {}
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    void instrFunc(llvm::Function &F);
    void instrLoad(llvm::LoadInst *load);
    void instrStore(llvm::StoreInst *store);
    void instrInstruction(llvm::Value *insid, llvm::Value *addr, 
           llvm::Value *data, llvm::Instruction *ins, bool after=true);
    void instrCall(llvm::CallInst *call);
    void instrInvoke(llvm::InvokeInst *invoke);
    void instrFirstNonPHI(llvm::Instruction *ins);
  };
}

#endif
