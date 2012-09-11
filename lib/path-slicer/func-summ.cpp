#include "ext-func-summ.h"
#include "func-summ.h"
//#include "tern/syncfuncs.h"
using namespace tern;

#include "llvm/Instructions.h"
using namespace llvm;

char tern::FuncSumm::ID = 0;

FuncSumm::FuncSumm(): FunctionPass(&ID) {
  idMgr = NULL;
  EM = NULL;
}

FuncSumm::~FuncSumm() {
  if (DBG) {
    tern::ExtFuncSumm::printAllSumm();  
    fprintf(stderr, "FuncSumm::~FuncSumm\n");
  }
}

void FuncSumm::init(InstrIdMgr *idMgr, Stat *stat, EventMgr *EM) {
  this->idMgr = idMgr;
  this->stat = stat;
  this->EM = EM;
}

void FuncSumm::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  FunctionPass::getAnalysisUsage(AU);
}

bool FuncSumm::runOnFunction(Function &F) {
  fprintf(stderr, "FuncSumm::collectInternalFunctions begin\n");
  // Record the main function.
  if (F.getNameStr() == "main" || F.getNameStr() == "__user_main")
    mainFunc = &F;

  // Collect internal function.
  if (!F.isDeclaration()) {
    internalFunctions.insert(&F);
    fprintf(stderr, "Function %s(%p) is internal.\n", 
      F.getNameStr().c_str(), (void *)&F);
  }
  fprintf(stderr, "FuncSumm::collectInternalFunctions end\n");
  return false;
}

bool FuncSumm::mayCallEvent(const llvm::Function *f) {
  long intF = (long)f;  
  return EM->mayCallEvent((Function *)intF);
}

bool FuncSumm::mayCallEvent(DynInstr *dynInstr) {
  Function *f = NULL;
  if (dynInstr) {
    DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
    f = callInstr->getCalledFunc();
  } else
    f = mainFunc;
  assert(f);
  return mayCallEvent(f);
}

bool FuncSumm::intraProcEventBetween(llvm::BranchInst *prevInstr, llvm::Instruction *postInstr) {
  return EM->intraProcEventBetween(prevInstr, postInstr);
}


bool FuncSumm::intraProcMayReachEvent(llvm::Instruction *instr) {
  return EM->intraProcMayReachEvent(instr);
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

/* Currently we used a string matching rather than a list of functions as the 
summary, because this appraoch is faster, and enough for use for all our applications.

I have checked the documents of all APIs in this cstring, none of the str* or 
mem* functions would modify errno.
http://www.cplusplus.com/reference/clibrary/cstring/
 
I have also checked the documents of all APIs in this cstrlib, for all the ato
* and str* functions, only strtod, strtol, and strtoul may modify errno.
http://www.cplusplus.com/reference/clibrary/cstdlib/
*/
bool FuncSumm::extCallMayModErrno(DynCallInstr *dynCallInstr) {
  assert(!isInternalCall(dynCallInstr));
  Function *called = dynCallInstr->getCalledFunc();
  std::string name = called->getNameStr();
  if (name.find("str") == 0) {
    if (name == "strtod" || name == "strtol" || name == "strtoul") // Only these 3 str* functions may modify errno.
      return true;
    else
      return false;
  } else if (name.find("mem") == 0) {
    return false;
  } else
    return true; // conservative.
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
  bool result = false;
  if (extLoadCache.in((void *)instr, (void *)argOffset, result))
    return result;
  
  assert(isa<CallInst>(instr));
  CallInst *ci = cast<CallInst>(instr);
  Function *f = ci->getCalledFunction();
  if (!f) {// Ignore function pointer for now, affect soundness.
    result = false;
    goto finish;
  }
  result = (ExtFuncSumm::getExtFuncSummType(f->getNameStr().c_str(), argOffset) ==
    ExtFuncSumm::ExtLoad);

finish:
  extLoadCache.add((void *)instr, (void *)argOffset, result);
  return result;
}

bool FuncSumm::isExtFuncSummStore(llvm::Instruction *instr, unsigned argOffset) {
  bool result = false;
  if (extStoreCache.in((void *)instr, (void *)argOffset, result))
    return result;
  
  assert(isa<CallInst>(instr));
  CallInst *ci = cast<CallInst>(instr);
  Function *f = ci->getCalledFunction();
  if (!f) {// Ignore function pointer for now, affect soundness.
    result = false;
    goto finish;
  }
  result = (ExtFuncSumm::getExtFuncSummType(f->getNameStr().c_str(), argOffset) ==
    ExtFuncSumm::ExtStore);

finish:
  extStoreCache.add((void *)instr, (void *)argOffset, result);
  return result;  
}

bool FuncSumm::extFuncHasSumm(llvm::Instruction *instr) {
  return extFuncHasLoadSumm(instr) || extFuncHasStoreSumm(instr);
}

bool FuncSumm::isEventCall(DynCallInstr *callInstr) {
  return EM->isEventCall(idMgr->getOrigInstr(callInstr));
}

size_t FuncSumm::numEventCallSites() {
  return EM->numEventCallSites();
}

void FuncSumm::printEventCalls(llvm::DenseSet<const Instruction *> *exedEvents) {
  return EM->printEventCalls(exedEvents);
}


