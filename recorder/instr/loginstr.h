/* -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGINSTR_H
#define __TERN_RECORDER_LOGINSTR_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/raw_ostream.h"

namespace tern {

struct LogInstr: public llvm::ModulePass {
  static char ID;
  llvm::TargetData *targetData;
  llvm::LLVMContext *context;

  const llvm::PointerType *addrType;
  const llvm::IntegerType *dataType;
  llvm::Value       *logInsid;
  llvm::Value       *logLoad;
  llvm::Value       *logStore;
  llvm::Value       *logCall;
  llvm::Value       *logRet;

  LogInstr(): ModulePass(&ID) {}
  virtual bool runOnModule(llvm::Module &M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  llvm::Value *castIfNecessary(llvm::Value *data, const llvm::Type *dst,
                                 llvm::Instruction *insert);
  void instrFunc(llvm::Function &F);
  void instrLoad(llvm::LoadInst *load);
  void instrStore(llvm::StoreInst *store);
  void instrLoadStore(llvm::Value *insid, llvm::Value *addr,
                      llvm::Value *data, llvm::Instruction *ins, bool isload);
  void instrCall(llvm::CallInst *call);
  void instrInvoke(llvm::InvokeInst *invoke);
  void instrFirstNonPHI(llvm::Instruction *ins);

  static inline void foo() {
  }
};

} // namespace tern

#endif
