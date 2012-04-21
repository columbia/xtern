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

std::string outputDir;

KleeTraceUtil::KleeTraceUtil() {
  kmodule = NULL;
}

KleeTraceUtil::~KleeTraceUtil() {

}

void KleeTraceUtil::init(InstrIdMgr *idMgr, Stat *stat) {
  this->idMgr = idMgr;
  this->stat = stat;
}

void KleeTraceUtil::initKModule(KModule *kmodule) {
  this->kmodule = kmodule;
}

void KleeTraceUtil::load(const char *tracePath, DynInstrVector *trace) {
  //NOP.
}

void KleeTraceUtil::store(unsigned testCaseId, const char *outputDir, DynInstrVector *trace) {
  char path[BUF_SIZE];
  memset(path, 0, BUF_SIZE);
  /* This format string had better be the same as the one in getTestFilename() 
  in main.cpp in klee so that the trace and err files are "ls" closed to each other. */
  snprintf(path, BUF_SIZE, "%s/test%06d.trace", outputDir, (int)testCaseId);
  fprintf(stderr, "Path %s\n", path);
  std::string ErrorInfo;
  raw_fd_ostream OS(path, ErrorInfo, raw_fd_ostream::F_Append);
  for (size_t i = 0; i < trace->size(); i++)
    stat->printDynInstr(OS, trace->at(i), "", true);
  OS.flush();
  OS.close();

  storeBr(testCaseId, outputDir, trace);
}

void KleeTraceUtil::storeBr(unsigned testCaseId, const char *outputDir, DynInstrVector *trace) {
  const static char *DSYM_TAG = "DIRECT_SYM";
  char path[BUF_SIZE];
  memset(path, 0, BUF_SIZE);
  /* This format string had better be the same as the one in getTestFilename() 
  in main.cpp in klee so that the trace and err files are "ls" closed to each other. */
  snprintf(path, BUF_SIZE, "%s/test%06d.trace.br", outputDir, (int)testCaseId);
  fprintf(stderr, "Path %s\n", path);
  std::string ErrorInfo;
  raw_fd_ostream OS(path, ErrorInfo, raw_fd_ostream::F_Append);
  bool newBr = false;
  for (size_t i = 0; i < trace->size(); i++) {
    DynInstr *dynInstr = trace->at(i);
    Instruction *instr = idMgr->getOrigInstr(dynInstr);
    if (!Util::isPHI(instr) && newBr) {
      OS << DSYM_TAG <<"-BR: INSTR-ID: " << dynInstr->getOrigInstrId() << "\n";
      newBr = false;
    }
    if (Util::isBr(instr))
      newBr = true;
  }
  OS.flush();
  OS.close();
}

void KleeTraceUtil::record(DynInstrVector *trace, void *instr, void *state, void *f) {
  record(trace, (KInstruction *)instr, (ThreadState *)state, (Function *)f);
}

void KleeTraceUtil::record(DynInstrVector *trace, KInstruction *kInstr,
  ThreadState *state, Function *f) {
  Instruction *instr = kInstr->inst;
  
  // Ignore an instruction if it is not from the original module.
  if (idMgr->getOrigInstrId(instr) == -1)
    return;

  //errs() << "KleeTraceUtil::record: " << stat->printInstr(instr) << "\n";
  // Real recording.
  if (Util::isPHI(instr)) {
    recordPHI(trace, kInstr, state);
  } else if (Util::isBr(instr)) {
    recordBr(trace, kInstr, state);
  }  else if (Util::isRet(instr)) {
    recordRet(trace, kInstr, state);
  }  else if (Util::isCall(instr)) {
    if (Util::isProcessExitFunc(f))
      recordProcessExitCall(trace, kInstr, state, f);
    else
      recordCall(trace, kInstr, state, f);
  } else if (Util::isMem(instr)) {
    if (Util::isLoad(instr))
      recordLoad(trace, kInstr, state);
    else if (Util::isStore(instr))
      recordStore(trace, kInstr, state);
  } else {
    recordNonMem(trace, kInstr, state);    
  }
}

void KleeTraceUtil::recordPHI(DynInstrVector *trace, klee::KInstruction *kInstr,
  ThreadState *state) {
  DynPHIInstr *phi = new DynPHIInstr;
  phi->setIndex(trace->size());
  phi->setTid((uchar)state->id);
  phi->setOrigInstrId(idMgr->getOrigInstrId(kInstr->inst));
  trace->push_back(phi);
  stat->collectExed(phi);
}

void KleeTraceUtil::recordBr(DynInstrVector *trace, klee::KInstruction *kInstr,
  ThreadState *state) {
  //rs() << "KleeTraceUtil::recordBr: " << ": " << *(kInstr->inst) << "\n";
  Instruction *instr = kInstr->inst;
  DynBrInstr *br = new DynBrInstr;
  br->setIndex(trace->size());
  br->setTid((uchar)state->id);
  br->setOrigInstrId(idMgr->getOrigInstrId(instr));
  if (!Util::isUniCondBr(instr))
    br->setBrCondition(eval(kInstr, 0, *state).value);  // The condition can be either symbolic or concrete. 
  trace->push_back(br);
  stat->collectExed(br);
}

void KleeTraceUtil::recordRet(DynInstrVector *trace, klee::KInstruction *kInstr,
  ThreadState *state) {
  DynRetInstr *ret = new DynRetInstr;
  ret->setIndex(trace->size());
  ret->setTid((uchar)state->id);
  ret->setOrigInstrId(idMgr->getOrigInstrId(kInstr->inst));
  trace->push_back(ret);
  stat->collectExed(ret);
}

void KleeTraceUtil::recordCall(DynInstrVector *trace, klee::KInstruction *kInstr,
  ThreadState *state, Function *f) {
  DynCallInstr *call = new DynCallInstr;
  call->setIndex(trace->size());
  call->setTid((uchar)state->id);
  call->setOrigInstrId(idMgr->getOrigInstrId(kInstr->inst));
  call->setCalledFunc(f);
  trace->push_back(call);
  stat->collectExed(call);
}

void KleeTraceUtil::recordProcessExitCall(DynInstrVector *trace, klee::KInstruction *kInstr, 
  klee::ThreadState *state, llvm::Function *f) {
  DynProcessExitCallInstr *exitCall = new DynProcessExitCallInstr;
  DynCallInstr *call = (DynCallInstr *)exitCall;
  call->setIndex(trace->size());
  call->setTid((uchar)state->id);
  call->setOrigInstrId(idMgr->getOrigInstrId(kInstr->inst));
  call->setCalledFunc(f);
  SERRS << "KleeTraceUtil::recordProcessExitCall " << (void *)f << ":" << f->getNameStr() << "\n";
  trace->push_back(call);
  stat->collectExed(call);
}

void KleeTraceUtil::recordNonMem(DynInstrVector *trace, KInstruction *kInstr,
  ThreadState *state) {
  DynInstr *instr = new DynInstr;
  instr->setIndex(trace->size());
  instr->setTid((uchar)state->id);
  instr->setOrigInstrId(idMgr->getOrigInstrId(kInstr->inst));
  trace->push_back(instr);
  stat->collectExed(instr);
}

void KleeTraceUtil::recordLoad(DynInstrVector *trace, KInstruction *kInstr,
  ThreadState *state) {
  DynMemInstr *load = new DynMemInstr;
  load->setIndex(trace->size());
  load->setTid((uchar)state->id);
  load->setOrigInstrId(idMgr->getOrigInstrId(kInstr->inst));
  load->setAddr(eval(kInstr, 0, *state).value);
  trace->push_back(load);
  stat->collectExed(load);
}

void KleeTraceUtil::recordStore(DynInstrVector *trace, KInstruction *kInstr,
  ThreadState *state) {
  DynMemInstr *store = new DynMemInstr;
  store->setIndex(trace->size());
  store->setTid((uchar)state->id);
  store->setOrigInstrId(idMgr->getOrigInstrId(kInstr->inst));
  store->setAddr(eval(kInstr, 1, *state).value);
  trace->push_back(store);    
  stat->collectExed(store);
}

/* Borrowed from the Executor.cpp in klee. */
const Cell& KleeTraceUtil::eval(KInstruction *ki, unsigned index, 
                           ThreadState &state) const {
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

void KleeTraceUtil::preProcess(DynInstrVector *trace) {
  // For each path, must clear ctx mgr.
  ctxMgr->clear();
  size_t traceSize = trace->size();
  dprint("KleeTraceUtil::preProcess trace size " SZ "\n", traceSize);
  
  for (size_t i = 0; i < traceSize; i++) {
    DynInstr *dynInstr = trace->at(i);
    Instruction *instr = idMgr->getOrigInstr(dynInstr);
    
    // (1) For each dynamic instruction, setup calling context.
    ctxMgr->setCallStack(dynInstr);

    // (2) For each dynamic phi instruction, setup incoming index.
    if (Util::isPHI(instr)) {
      DynInstr *prevInstr = NULL;
      for (int j = i - 1; j >= 0; j--) {
        prevInstr = trace->at(j);
        if (Util::isBr(idMgr->getOrigInstr(prevInstr)) &&
          prevInstr->getTid() == dynInstr->getTid())
          break;
      }
      assert(prevInstr);
      BasicBlock *bb = Util::getBasicBlock(idMgr->getOrigInstr(prevInstr));
      PHINode *phi = dyn_cast<PHINode>(instr);
      int idx = phi->getBasicBlockIndex(bb);
      assert(idx != -1);
      ((DynPHIInstr *)dynInstr)->setIncomingIndex((uchar)idx);
    }

    // (3) For each branch instruction, get its successor basic block.
    if (Util::isBr(instr)) {
      DynInstr *nextInstr = NULL;
      for (size_t j = i+1; j < traceSize; j++) {
        nextInstr = trace->at(j);
        if (nextInstr->getTid() == dynInstr->getTid())
          break;
      }
      assert(nextInstr || i == traceSize - 1);  // The branch must have successor, or it is the last instruction in the trace.
      BasicBlock *successor = NULL;
      if (nextInstr)
        successor = Util::getBasicBlock(idMgr->getOrigInstr(nextInstr));
      ((DynBrInstr *)dynInstr)->setSuccessorBB(successor);
    }
    
    // (4) For each dynamic ret instruction, setup its dynamic call instruction.
    if (Util::isRet(instr)) {
      DynRetInstr *ret = (DynRetInstr *)dynInstr;
      DynCallInstr *call = ctxMgr->getCaller(dynInstr);
      if (!call)
        assert(ret->getCallingCtx()->size() == 0);
      else 
        ret->setDynCallInstr(call);
    }
    
    // (5) For each dynamic pthread_create(), setup its child thread id.
    if (Util::isCall(instr)) {
      DynCallInstr *call = (DynCallInstr *)dynInstr;
      if (Util::isThreadCreate(call))
        ((DynSpawnThreadInstr *)call)->setChildTid(call->getTid()+1);
      if (Util::isProcessExitFunc(call->getCalledFunc()))
        SERRS << "\n\nprocessing exit function... " << (void *)call->getCalledFunc() << "\n\n";
      DynCallInstr *caller = ctxMgr->getCaller(dynInstr);
      if (!caller)
        assert(call->getCallingCtx()->size() == 0);
      else 
        call->setCaller(caller);

      /* Must make sure Gang's thread model in KLEE still make this rule hold:
      a child's tid is the parent's +1. */
    }

    // (6) Update per-tid callstack. This must happen after (3), which calls getCallOfRet().
    if (Util::isCall(instr) || Util::isRet(instr))
      ctxMgr->updateCallStack(dynInstr);

    if (DBG)
      stat->printDynInstr(dynInstr, __func__, true);
  }
}

/* NOTE: directly freeing the dynamic instructions is incorrect, since some 
instructions are shared by multiple states. Can use ref cnt mechanism to solve 
this problem later. */
void KleeTraceUtil::postProcess(DynInstrVector *trace) {
  /*for (size_t i = 0; i < trace->size(); i++) {
    DynInstr *dynInstr = trace->at(i);
    stat->printDynInstr(dynInstr, __func__);
    delete dynInstr;
  }
  trace->clear();*/

  /* After slicing on current path, must reset all the taken flags for instructions,
  otherwise further slicing on other paths would have logical corruptions. */
  for (size_t i = 0; i < trace->size(); i++) {
    DynInstr *dynInstr = trace->at(i);
    if (!dynInstr->isTarget())
      dynInstr->setTaken(TakenFlags::NOT_TAKEN);
  }
}


