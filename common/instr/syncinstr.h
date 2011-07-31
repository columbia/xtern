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
  SyncInstr(): ModulePass(&ID) {}
  virtual bool runOnModule(llvm::Module &M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  llvm::Module *getTemplateModule(void);
  void initFuncTypes(llvm::Module &M);
  void replaceFunctionInCall(llvm::Module &M, llvm::Instruction *I, unsigned syncid);
  void warnEscapeFuncs(llvm::Module &M);

  typedef std::tr1::unordered_map<unsigned,
            const llvm::FunctionType*> functype_map_t;
  functype_map_t functypes;

  static char ID;
};

} // namespace tern

#endif
