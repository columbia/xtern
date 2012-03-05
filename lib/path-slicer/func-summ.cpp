#include "ext-func-summ.h"
#include "func-summ.h"
//#include "tern/syncfuncs.h"
using namespace tern;

#include "llvm/Instructions.h"
using namespace llvm;

char tern::FuncSumm::ID = 0;

FuncSumm::FuncSumm(): ModulePass(&ID) {
}

FuncSumm::~FuncSumm() {
  if (DBG) {
    tern::ExtFuncSumm::printAllSumm();  
    fprintf(stderr, "FuncSumm::~FuncSumm\n");
  }
}

void FuncSumm::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<EventMgr>();
  ModulePass::getAnalysisUsage(AU);
}

bool FuncSumm::runOnModule(Module &M) {
  collectInternalFunctions(M);
  return false;
}

bool FuncSumm::mayCallEvent(const llvm::Function *f) {
  long intF = (long)f;  
  EventMgr &EM = getAnalysis<EventMgr>();
  return EM.mayCallEvent((Function *)intF);
}

bool FuncSumm::mayCallEvent(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  Function *f = callInstr->getCalledFunc();
  assert(f);
  return mayCallEvent(f);
}

bool FuncSumm::eventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr) {
  EventMgr &EM = getAnalysis<EventMgr>();
  return EM.eventBetween(prevInstr, postInstr);
}

void FuncSumm::collectInternalFunctions(Module &M) {
  fprintf(stderr, "FuncSumm::collectInternalFunctions begin\n");
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    if (!f->isDeclaration()) {
      internalFunctions.insert(f);
      fprintf(stderr, "Function %s(%p) is internal.\n", 
        f->getNameStr().c_str(), (void *)f);
    }
  }
  fprintf(stderr, "FuncSumm::collectInternalFunctions end\n");
}

bool FuncSumm::isInternalCall(const Instruction *instr) {
  const CallInst *ci = dyn_cast<CallInst>(instr);
  assert(ci);
  const Function *f = ci->getCalledFunction();  
  if (!f) // If it is an indirect call (function pointer), return false conservatively.
    return false;
  else
    return isInternalFunction(f);
}

bool FuncSumm::isInternalCall(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  Function *f = callInstr->getCalledFunc();
  return isInternalFunction(f);
}

bool FuncSumm::isInternalFunction(const Function *f) {
  // TBD: If the called function is a function pointer, would "f" be NULL?
  if (!f)
    return false;
  bool result = !f->isDeclaration() && internalFunctions.count(f) > 0;
  return result;
}

bool FuncSumm::extFuncHasLoadSumm(llvm::Instruction *instr) {
  assert(isa<CallInst>(instr));
  CallInst *ci = cast<CallInst>(instr);
  Function *f = ci->getCalledFunction();
  if (!f) // Ignore function pointer for now, affect soundness.
    return false;
  return ExtFuncSumm::extFuncHasLoadSumm(f->getNameStr().c_str());
}

bool FuncSumm::extFuncHasStoreSumm(llvm::Instruction *instr) {
  assert(isa<CallInst>(instr));
  CallInst *ci = cast<CallInst>(instr);
  Function *f = ci->getCalledFunction();
  if (!f) // Ignore function pointer for now, affect soundness.
    return false;
  return ExtFuncSumm::extFuncHasStoreSumm(f->getNameStr().c_str());
}

bool FuncSumm::isExtFuncSummLoad(llvm::Instruction *instr, unsigned argOffset) {
  assert(isa<CallInst>(instr));
  CallInst *ci = cast<CallInst>(instr);
  Function *f = ci->getCalledFunction();
  if (!f) // Ignore function pointer for now, affect soundness.
    return false;
  return ExtFuncSumm::getExtFuncSummType(f->getNameStr().c_str(), argOffset) ==
    ExtFuncSumm::ExtLoad;
}

bool FuncSumm::isExtFuncSummStore(llvm::Instruction *instr, unsigned argOffset) {
  assert(isa<CallInst>(instr));
  CallInst *ci = cast<CallInst>(instr);
  Function *f = ci->getCalledFunction();
  if (!f) // Ignore function pointer for now, affect soundness.
    return false;
  return ExtFuncSumm::getExtFuncSummType(f->getNameStr().c_str(), argOffset) ==
    ExtFuncSumm::ExtStore;
}

bool FuncSumm::extFuncHasSumm(llvm::Instruction *instr) {
  return extFuncHasLoadSumm(instr) || extFuncHasStoreSumm(instr);
}

