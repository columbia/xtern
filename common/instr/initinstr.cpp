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

// adapted from llvm::InsertProfilingShutdownCall.
void InitInstr::addCtorOrDtor(Module *M, Function *F, bool isCtor) {
  const char* VarName = (isCtor? "llvm.global_ctors" : "llvm.global_dtors");

  // llvm.global_dtors is an array of type { i32, void ()* }. Prepare
  // those types.
  vector<const Type*> GlobalElems(2);
  GlobalElems[0] = Type::getInt32Ty(M->getContext());
  GlobalElems[1] =
    FunctionType::get(Type::getVoidTy(M->getContext()), false)->getPointerTo();

  StructType *GlobalElemTy =
    StructType::get(M->getContext(), GlobalElems, false);

  // Construct the new element we'll be adding.
  vector<Constant*> Elem(2);
  Elem[0] = ConstantInt::get(Type::getInt32Ty(M->getContext()), 65535);
  Elem[1] = ConstantExpr::getBitCast(F, GlobalElems[1]);

  int nElem = 1;
  std::vector<Constant *> cdtors;

  // If llvm.global_dtors exists, make a copy of the things in its list and
  // delete it, to replace it with one that has a larger array type.
  if (GlobalVariable *Global = M->getNamedGlobal(VarName)) {
    if (ConstantArray *InitList =
        dyn_cast<ConstantArray>(Global->getInitializer())) {
      nElem += InitList->getType()->getNumElements();
      for (unsigned i = 0, e = InitList->getType()->getNumElements();
           i != e; ++i)
        cdtors.push_back(cast<Constant>(InitList->getOperand(i)));
    }
    Global->eraseFromParent();
  }

  // Build up llvm.global_ctors or llvm.global.dtors with our new item in it.
  GlobalVariable *Global = new GlobalVariable(
      *M, ArrayType::get(GlobalElemTy, nElem), false,
      GlobalValue::AppendingLinkage, NULL, VarName);

  // append as last ctor or dtor; seems that the last one is
  // invoked first.
  //
  // FIXME: what is the static ctor or dtor invoke order in LLVM?
  cdtors.push_back(ConstantStruct::get(GlobalElemTy, Elem));

  Global->setInitializer(ConstantArray::get(
      cast<ArrayType>(Global->getType()->getElementType()), cdtors));
}


/// add __tern_prog_begin() as the first static ctor, and __tern_prog_end
/// as the first static dtor of @M
void InitInstr::addBeginEndAsCtorDtor(Module &M, GlobalValue* GCL) {

  const FunctionType *Ty = TypeBuilder<void (void),
    false>::get(getGlobalContext());
  Constant *C;
  Function *beginF, *endF;

  C = M.getOrInsertFunction("__tern_prog_begin", Ty);
  beginF = dyn_cast<Function>(C);
  assert(beginF && "can't insert __tern_prog_begin function!");
  C = M.getOrInsertFunction("__tern_prog_end", Ty);
  endF = dyn_cast<Function>(C);
  assert(endF && "can't insert __tern_prog_end function!");

  addCtorOrDtor(&M, beginF, true);
  addCtorOrDtor(&M, endF, false);
}

/// after transformation, main() in user program is renamed to
// __user_main_for_tern(), and a new main() stub is created as
///     main() {
///        __tern_prog_begin();
///        __user_main_for_tern();
///        __tern_prog_end();
///
void InitInstr::addBeginEndInMain(llvm::Module &M, llvm::Function *mainfunc) {
  const FunctionType *Ty = TypeBuilder<void (void),
    false>::get(getGlobalContext());
  Constant *C = M.getOrInsertFunction("__tern_prog_begin", Ty);
  Function *F = dyn_cast<Function>(C);
  assert(F && "can't insert function!");

  Function *userMain = M.getFunction("main");
  assert(userMain && "unable to get user main");
  userMain->setName("__user_main_for_tern");
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
  Constant *C2 = M.getOrInsertFunction("__tern_prog_end", Ty);
  Function *F2 = dyn_cast<Function>(C2);
  assert(F && "can't insert function!");
  CallInst::Create(F2, "", BB);

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
  GlobalVariable *GDL = M.getGlobalVariable("llvm.global_dtors");
  if(GCL || GDL)
    addBeginEndAsCtorDtor(M, GCL);
  else
    addBeginEndInMain(M, mainfunc);

  return true;
}

} // namespace tern

namespace {

static RegisterPass<tern::InitInstr>
X("initinstr", "instrumentation of tern initialization", false, false);

}
