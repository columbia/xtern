/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_INITINSTR_H
#define __TERN_COMMON_INITINSTR_H

#include "common/util.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"

namespace tern {

struct InitInstr: public llvm::ModulePass {
  InitInstr(): ModulePass(&ID) {}
  virtual bool runOnModule(llvm::Module &M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  void addBeginEndAsCtorDtor(llvm::Module &M);
  void addBeginEndInMain(llvm::Module &M, llvm::Function *mainfunc,
                         bool add_end=true);
  void addSymbolicArgv(llvm::Module &M, llvm::Function *mainfunc);
  void addSymbolic(llvm::Module &M);
  void addCtorOrDtor(llvm::Module *M, llvm::Function *F, bool isCtor);

  static char ID;
};

} // namespace tern

#endif
