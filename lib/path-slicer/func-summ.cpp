
#include "func-summ.h"
using namespace tern;

#include "llvm/Instructions.h"
using namespace llvm;

FuncSumm::FuncSumm() {
  
}

FuncSumm::~FuncSumm() {

}

void FuncSumm::init() {
  
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
  
  fprintf(stderr, "Function %s(%p) is isInternalFunction?\n", 
    f->getNameStr().c_str(), (void *)f);
  bool result = !f->isDeclaration() && internalFunctions.count(f) > 0;
  fprintf(stderr, "Function %s is isInternalFunction %d.\n", 
    f->getNameStr().c_str(), result);
  return result;
}

void FuncSumm::addInternelFunction(const llvm::Function *f) {
  internalFunctions.insert(f);
}

