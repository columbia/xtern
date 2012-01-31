#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "klee-trace-util.h"
#include "util.h"
#include "dyn-instrs.h"
using namespace tern;

using namespace klee;

using namespace llvm;

KleeTraceUtil::KleeTraceUtil() {
  kmodule = NULL;
  trace = NULL;
}

KleeTraceUtil::~KleeTraceUtil() {

}

void KleeTraceUtil::initKModule(KModule *kmodule) {
  this->kmodule = kmodule;
  idAssigner = new IDAssigner();
  PassManager *pm = new PassManager;
  pm->add(idAssigner);
  pm->run(*(kmodule->module));
}

void KleeTraceUtil::load(const char *tracePath, DynInstrVector *trace) {
  //NOP.
}

void KleeTraceUtil::store(const char *tracePath) {
  //NOP.
}

void KleeTraceUtil::record(void *instr, void *state, void *f) {
  record((KInstruction *)instr, (ExecutionState *)state, (Function *)f);
}

void KleeTraceUtil::record(KInstruction *kInstr, ExecutionState *state, Function *f) {
  Instruction *instr = kInstr->inst;
  if (Util::isPHI(instr)) {
    recordPHI(kInstr, state);
  } else if (Util::isBr(instr)) {
    recordBr(kInstr, state);
  }  else if (Util::isRet(instr)) {
    recordRet(kInstr, state);
  }  else if (Util::isCall(instr)) {
    recordCall(kInstr, state, f);
  } else if (Util::isMem(instr)) {
    if (Util::isLoad(instr))
      recordLoad(kInstr, state);
    else if (Util::isStore(instr))
      recordStore(kInstr, state);
  } else {
    recordNonMem(kInstr, state);    
  }
}

void KleeTraceUtil::recordPHI(klee::KInstruction *kInstr,
  klee::ExecutionState *state) {
  DynPHIInstr *phi = new DynPHIInstr;
  phi->setIndex(trace->size());
  trace->push_back(phi);
}

void KleeTraceUtil::recordBr(klee::KInstruction *kInstr,
  klee::ExecutionState *state) {
  DynBrInstr *br = new DynBrInstr;
  br->setIndex(trace->size());
  trace->push_back(br);
}

void KleeTraceUtil::recordRet(klee::KInstruction *kInstr,
  klee::ExecutionState *state) {
  DynRetInstr *ret = new DynRetInstr;
  ret->setIndex(trace->size());
  trace->push_back(ret);
}

void KleeTraceUtil::recordCall(klee::KInstruction *kInstr,
  klee::ExecutionState *state, Function *f) {
  DynCallInstr *call = new DynCallInstr;
  call->setIndex(trace->size());
  call->setCalledFunc(f);
  trace->push_back(call);
}

void KleeTraceUtil::recordNonMem(KInstruction *kInstr,
  ExecutionState *state) {
  DynInstr *instr = new DynInstr;
  instr->setIndex(trace->size());
  trace->push_back(instr);  
}

void KleeTraceUtil::recordLoad(KInstruction *kInstr,
  ExecutionState *state) {
  DynMemInstr *load = new DynMemInstr;
  load->setIndex(trace->size());
  load->setConAddr(0);// TBD: add sym/concrete hybrid execution and get concrete mem addr.
  load->setSymAddr(eval(kInstr, 0, *state).value);
  trace->push_back(load);    
}

void KleeTraceUtil::recordStore(KInstruction *kInstr,
  ExecutionState *state) {
  DynMemInstr *store = new DynMemInstr;
  store->setIndex(trace->size());
  store->setConAddr(0);// TBD: add sym/concrete hybrid execution and get concrete mem addr.
  store->setSymAddr(eval(kInstr, 1, *state).value);
  trace->push_back(store);      
}

/* Borrowed from the Executor.cpp in klee. */
const Cell& KleeTraceUtil::eval(KInstruction *ki, unsigned index, 
                           ExecutionState &state) const {
  assert(index < ki->inst->getNumOperands());
  int vnumber = ki->operands[index];

  assert(vnumber != -1 &&
         "Invalid operand to eval(), not a value or constant!");

  // Determine if this is a constant or not.
  if (vnumber < 0) {
    unsigned index = -vnumber - 2;
    return kmodule->constantTable[index];
  } else {
    unsigned index = vnumber;
    StackFrame &sf = state.stack.back();
    return sf.locals[index];
  }
}

void KleeTraceUtil::initTrace(DynInstrVector *trace) {
  this->trace = trace;
}

void KleeTraceUtil::processTrace() {
  /* TBD: 
    (1) Fop each dynamic phi instruction, setup incoming index.
    (2) For each dynamic instruction, setup calling context.
    (3) For each dynamic ret instruction, setup its dynamic call instruction. 

  */

  // Debug print.
  for (size_t i = 0; i < trace->size(); i++) {
    DynInstr *dynInstr = trace->at(i);
    Instruction *instr = idAssigner->getInstruction(dynInstr->getOrigInstrId());
    fprintf(stderr, "INDEX " SZ ", THREAD-ID: %d, INSTR-ID: %d, OP: %s\n",
      dynInstr->getIndex(), dynInstr->getTid(), dynInstr->getOrigInstrId(),
      instr->getOpcodeName());
  }
}


