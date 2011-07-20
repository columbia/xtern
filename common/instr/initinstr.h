/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_INITINSTR_H
#define __TERN_COMMON_INITINSTR_H

#include "util.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"

namespace tern {

struct InitInstr: public llvm::ModulePass {
  InitInstr(bool uclibcEnabled): ModulePass(&ID), uclibc(uclibcEnabled) {}
  virtual bool runOnModule(llvm::Module &M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  void addBeginEndAsCtorDtor(llvm::Module &M);
  void addBeginEndInMain(llvm::Module &M, llvm::Function *mainfunc);
  void addSymbolicArgv(llvm::Module &M, llvm::Function *mainfunc);
  void addSymbolic(llvm::Module &M);
  void addCtorOrDtor(llvm::Module *M, llvm::Function *F, bool isCtor);

  /// if uclibc is enabled, wrap main(), instead of insert tern runtime
  /// init/shutdown code as static ctor/dtor.  SyncInstr must also replace
  /// exit() with tern_exit()
  bool uclibc;
  static char ID;
};

} // namespace tern

#endif
