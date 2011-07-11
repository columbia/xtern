/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_INITINSTR_H
#define __TERN_COMMON_INITINSTR_H

#include "util.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"

namespace tern {

struct InitInstr: public llvm::ModulePass {
  InitInstr(): ModulePass(&ID) {}
  virtual bool runOnModule(llvm::Module &M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  void addInitAsCtor(llvm::Module &M, llvm::GlobalValue* GCL);
  void addInitInMain(llvm::Module &M, llvm::Function *mainfunc);
  void addSymbolicArgv(llvm::Module &M, llvm::Function *mainfunc);
  void addSymbolic(llvm::Module &M);

  static char ID;
};

} // namespace tern

#endif
