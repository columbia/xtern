/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_RECINSTR_H
#define __TERN_RECORDER_RECINSTR_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/raw_ostream.h"

namespace tern 
{
  struct RecInstr: public llvm::ModulePass 
  {
    static char ID;
    llvm::TargetData *targetData;
    llvm::LLVMContext *context;

    enum {MaxTypeNum = 4};
    const llvm::PointerType *addrType;
    const llvm::IntegerType *dataTypes[MaxTypeNum];
    const llvm::Value *logFuncs[MaxTypeNum];

    RecInstr(): ModulePass(&ID) {}
    virtual bool runOnModule(llvm::Module &M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    void instrFunc(llvm::Function &F);
    bool instrLoad(llvm::LoadInst *load);
    bool instrStore(llvm::StoreInst *store);
    void instrLoadOrStore(llvm::Value *insid, llvm::Value *addr, 
                          llvm::Value *data, llvm::Instruction *ins);
    bool instrCall(llvm::CallInst *call);
    bool instrInvoke(llvm::InvokeInst *invoke);
    
    const llvm::IntegerType *getIntTy(unsigned width);
    const llvm::FunctionType *getFuncTy(unsigned width);
    llvm::Value* getInsID(const llvm::Instruction *I) const;
  };
}

#endif
