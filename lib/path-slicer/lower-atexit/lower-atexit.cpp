#include "lower-atexit.h"
using namespace tern;

#include "llvm/Instructions.h"
#include "llvm/Support/CallSite.h"
using namespace llvm;

using namespace std;

static RegisterPass<LowerAtExitPass> X(
		"lower-atexit",
		"Transform atext(...) to be the real calls before all exit().",
		false,
		true); // is analysis

char LowerAtExitPass::ID = 0;

LowerAtExitPass::LowerAtExitPass(): ModulePass(&ID) {
}

LowerAtExitPass::~LowerAtExitPass() {}

void LowerAtExitPass::print(raw_ostream &O, const Module *M) const {

}

void LowerAtExitPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

bool LowerAtExitPass::runOnModule(Module &M) {
  Function *f = M.getFunction("exit");
  if (f) {
    fprintf(stderr, "Renaming exit to be exit ternExit...");
    f->setName("ternExit");
  }
  return true;
  bool result = collectAtExitFunctions(M);
  if (result) {
    insertCallsBeforeExit(M);
    deleteAtExitCalls(M);
  }
  return result;
}

bool LowerAtExitPass::collectAtExitFunctions(llvm::Module &M) {
  bool result = false;
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (isa<CallInst>(i)) {
          CallInst *ci = cast<CallInst>(i);
          Function *f = ci->getCalledFunction();
          if (f && f->getNameStr() == "atexit") {
            CallSite cs  = CallSite(ci);
            Value *v = *cs.arg_begin();
            assert(isa<Function>(v) && "Called value in atexit() must be a function.");
            atExitFuncs.push_back(cast<Function>(v));
            rmAtExitCalls.push_back(i);
            result = true;
          }
        }
      }
    }
  }
  return result;
}

void LowerAtExitPass::insertCallsBeforeExit(llvm::Module &M) {
  vector<Value*> args;
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (isa<CallInst>(i)) {
          CallInst *ci = cast<CallInst>(i);
          Function *f = ci->getCalledFunction();
          if (f && f->getNameStr() == "exit") {
            for (int n = (int)atExitFuncs.size() - 1; n >= 0; n--)
              CallInst::Create(atExitFuncs[n], args.begin(), args.end(), "", i);
          }
        }
      }
    }
  }
}

void LowerAtExitPass::deleteAtExitCalls(llvm::Module &M) {
  for (size_t i = 0; i < rmAtExitCalls.size(); i++)
    rmAtExitCalls[i]->eraseFromParent();
}

