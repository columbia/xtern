/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_COMMON_SYNCINSTR_H
#define __TERN_COMMON_SYNCINSTR_H

#include <tr1/unordered_map>
#include "util.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/raw_ostream.h"

namespace tern {

struct SyncInstr: public llvm::ModulePass {
  SyncInstr(bool uclibcEnabled): ModulePass(&ID), uclibc(uclibcEnabled) {}
  virtual bool runOnModule(llvm::Module &M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  llvm::Module *getTemplateModule(void);
  void initFuncTypes(llvm::Module &M);
  void replaceFunctionInCall(llvm::Module &M, llvm::Instruction *I, unsigned syncid);
  void warnEscapeFuncs(llvm::Module &M);

  typedef std::tr1::unordered_map<unsigned,
            const llvm::FunctionType*> functype_map_t;
  functype_map_t functypes;

  /// if uclibc is enabled, replace exit(status) with tern_exit(insid,
  /// status).  The reason is, with uclibc, InitStr has to wrap main()
  /// instead of inserting tern init and shutdown code as static ctor or
  /// dtor.  Therefore, we must replace exit() with tern_exit(), so that
  /// we get a chance to call tern_prog_end() before the program exits
  bool uclibc;
  static char ID;
};

} // namespace tern

#endif
