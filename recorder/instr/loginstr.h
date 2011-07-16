/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#ifndef __TERN_RECORDER_LOGINSTR_H
#define __TERN_RECORDER_LOGINSTR_H

#include <tr1/unordered_map>
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

  typedef std::tr1::unordered_map<llvm::Function*, unsigned> func_map_t;
  func_map_t       escapes;
  func_map_t       loggables;
  std::string      func_map_file;

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
  void instrCall(llvm::Instruction *call);
  void instrInvoke(llvm::InvokeInst *invoke);
  void instrFirstNonPHI(llvm::Instruction *ins);

  /// get loggable callees (i.e., calls to them must be logged); within
  /// these functions, also computes escape functions (function address
  /// taken) so that we can look for them when logging indirect calls.
  void getFuncs(llvm::Module &M);
  void markFuncs(llvm::Module &M);
  void exportFuncs(void);
  void setFuncsExportFile(const std::string& file) { func_map_file = file; }
};

} // namespace tern

#endif
