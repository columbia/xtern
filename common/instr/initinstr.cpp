/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <vector>
#include "config.h"
#include "initinstr.h"
#include "instrutil.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TypeBuilder.h"

using namespace std;
using namespace llvm;

namespace tern {

char InitInstr::ID = 0;

void InitInstr::addBeginEndAsCtor(llvm::Module &M, llvm::GlobalValue* GCL) {
  // TODO: add a call to __tern_prog_begin as the first static constructor
  assert(0 && "static constructors are not currently supported!");

// ./Transforms/IPO/GlobalOpt.cpp

#ifndef ENABLE_ATEXIT
  // refer to llvm::InsertProfilingShutdownCall
#endif
}

void InitInstr::addBeginEndInMain(llvm::Module &M, llvm::Function *mainfunc) {
  const FunctionType *Ty = TypeBuilder<void (void),
    false>::get(getGlobalContext());
  Constant *C = M.getOrInsertFunction("__tern_prog_begin", Ty);
  Function *F = dyn_cast<Function>(C);
  assert(F && "can't insert function!");

  Function *userMain = M.getFunction("main");
  assert(userMain && "unable to get user main");
  userMain->setName("__main_in_tern");
  const FunctionType *mainTy = userMain->getFunctionType();
  Function *stub = Function::Create(mainTy, GlobalVariable::ExternalLinkage,
                                    "main", &M);
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", stub);
  CallInst::Create(F, "", BB);
  vector<Value*> args;
  for(Function::arg_iterator ai=stub->arg_begin(); ai!=stub->arg_end(); ++ai)
    args.push_back(ai);
  Value *ret = CallInst::Create(userMain, args.begin(), args.end(),
                                "orig_main", BB);
#ifndef ENABLE_ATEXIT
  Constant *C2 = M.getOrInsertFunction("__tern_prog_end", Ty);
  Function *F2 = dyn_cast<Function>(C2);
  assert(F && "can't insert function!");
  CallInst::Create(F2, "", BB);
#endif

  ReturnInst::Create(getGlobalContext(), ret, BB);
}

void InitInstr::addSymbolicArgv(llvm::Module &M, llvm::Function *mainfunc) {

  // TODO: need to assign the inserted __tern_symbolic_argv an instruction ID
  return;

  Instruction *I = mainfunc->getEntryBlock().getFirstNonPHI();
  const FunctionType *Ty = TypeBuilder<void (types::i<32>, types::i<32>,
                                             types::i<8>**),
    false>::get(getGlobalContext());
  Constant *C = M.getOrInsertFunction("__tern_symbolic_argv", Ty);
  Function *F = dyn_cast<Function>(C);
  assert(F && "can't insert function!");
  SmallVector<Value*, 8> args;
  args.push_back(getInsID(I));
  for(Function::arg_iterator ai=mainfunc->arg_begin(), ae=mainfunc->arg_end();
      ai != ae; ++ai) {
    assert(ai);
    args.push_back(ai);
  }
  CallInst::Create(F, args.begin(), args.end(), "", I);
}

void InitInstr::addSymbolic(llvm::Module &M) {
  // TODO.  add a call to __tern_symbolic for each read() (for batch
  // programs) and recv() (for server programs).
}

void InitInstr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  ModulePass::getAnalysisUsage(AU);
}

bool InitInstr::runOnModule(Module &M) {
  Function *mainfunc = M.getFunction("main");
  assert(mainfunc && "can't find main()!");

  if(mainfunc->arg_size())
    addSymbolicArgv(M, mainfunc);
  addSymbolic(M);

  GlobalVariable *GCL = M.getGlobalVariable("llvm.global_ctors");
  if(GCL)
    addBeginEndAsCtor(M, GCL);
  else
    addBeginEndInMain(M, mainfunc);

  return true;
}

} // namespace tern

namespace {

static RegisterPass<tern::InitInstr>
X("initinstr", "instrumentation of tern initialization", false, false);

}
