/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */

#include <vector>
#include "initinstr.h"
#include "instrutil.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TypeBuilder.h"

using namespace std;
using namespace llvm;

namespace tern {

char InitInstr::ID = 0;

void InitInstr::addInitAsCtor(llvm::Module &M, llvm::GlobalValue* GCL) {
  // TODO: add a call to __tern_prog_begin as the first static constructor
}

void InitInstr::addInitInMain(llvm::Module &M, llvm::Function *mainfunc) {
  Instruction *I = mainfunc->getEntryBlock().getFirstNonPHI();
  const FunctionType *Ty = TypeBuilder<void (void),
    false>::get(getGlobalContext());
  Constant *C = M.getOrInsertFunction("__tern_prog_begin", Ty);
  Function *F = dyn_cast<Function>(C);
  assert(F && "can't insert function!");
  CallInst::Create(F, "", I);
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
    addInitAsCtor(M, GCL);
  else
    addInitInMain(M, mainfunc);

  return true;
}

} // namespace tern

namespace {

static RegisterPass<tern::InitInstr>
X("initinstr", "instrumentation of tern initialization", false, false);

}
