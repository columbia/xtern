
#include "func-summ.h"
#include "tern/syncfuncs.h"
using namespace tern;

#include "llvm/Instructions.h"
using namespace llvm;

char tern::FuncSumm::ID = 0;

FuncSumm::FuncSumm(): ModulePass(&ID) {
  
}

FuncSumm::~FuncSumm() {

}

void FuncSumm::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<EventFunc>();
  ModulePass::getAnalysisUsage(AU);
}

bool FuncSumm::runOnModule(Module &M) {
  collectInternalFunctions(M);
  return false;
}

void FuncSumm::initEvents(Module &M) {
  vector<Function *> eventList;
  // Get function list from syncfuncs.h.
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    if (f->hasName()) {
      unsigned nr = tern::syncfunc::getNameID(f->getNameStr().c_str());
      if (tern::syncfunc::isSync(nr))
        eventList.push_back(f);
    }
  }
  
  // Add all sync operations from syncfunc.cpp in xtern or fopen()/fclose() APIs.
  EventFunc &EF = getAnalysis<EventFunc>();
  EF.setupEvents(eventList);
}

bool FuncSumm::isEventFunction(const llvm::Function *f) {
  long intF = (long)f;  
  EventFunc &EF = getAnalysis<EventFunc>();
  return EF.contains_sync_operation((Function *)intF);
}

bool FuncSumm::isEventCall(const llvm::Instruction *instr) {
  return false; // TBD
}

bool FuncSumm::isEventCall(DynInstr *dynInstr) {
  return false; // TBD
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
  
  fprintf(stderr, "Function %s(%p) is isInternalFunction?\n", 
    f->getNameStr().c_str(), (void *)f);
  bool result = !f->isDeclaration() && internalFunctions.count(f) > 0;
  fprintf(stderr, "Function %s is isInternalFunction %d.\n", 
    f->getNameStr().c_str(), result);
  return result;
}

