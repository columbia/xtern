#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "klee-trace-util.h"
#include "util.h"
using namespace tern;

using namespace klee;

InstrRecord::InstrRecord() {
  tid = instrId = -1;
}

InstrRecord::~InstrRecord() {

}

void InstrRecord::setTid(int tid) {
  this->tid = tid;
}

int InstrRecord::getTid() {
  return tid;
}

void InstrRecord::setInstrId(int instrId) {
  this->instrId = instrId;
}

int InstrRecord::getInstrId() {
  return instrId;
}

MemInstrRecord::MemInstrRecord() {

}

MemInstrRecord::~MemInstrRecord() {
  
}

void MemInstrRecord::setAddr(klee::ref<klee::Expr> addr) {
  this->addr = addr;
}

klee::ref<klee::Expr> MemInstrRecord::getAddr() {
  return addr;
}

bool MemInstrRecord::isAddrConstant() {
  return isa<klee::ConstantExpr>(addr);
}

KleeTraceUtil::KleeTraceUtil() {
  kmodule = NULL;
  fd = -1;
  offset = 0;
}

KleeTraceUtil::~KleeTraceUtil() {

}

void KleeTraceUtil::initKModule(KModule *kmodule) {
  this->kmodule = kmodule;
}

void KleeTraceUtil::load(const char *tracePath, DynInstrVector *trace) {
  mapTrace(tracePath);
  // TBD: LOOP TO READ BACK TRACE.
}

void KleeTraceUtil::store(const char *tracePath) {
  
}

void KleeTraceUtil::record(void *instr, void *state) {
  record((KInstruction *)instr, (ExecutionState *)state);
}

void KleeTraceUtil::record(KInstruction *kInstr, ExecutionState *state) {
  Instruction *instr = kInstr->inst;
  if (Util::isLoad(instr)) {
    recordLoad(kInstr, state);
  } else if (Util::isStore(instr)) {
    recordStore(kInstr, state);
  } else {
    recordNonMem(kInstr, state);
  }
}

void KleeTraceUtil::recordLoad(KInstruction *kInstr, ExecutionState *state) {
  checkOffset();
  MemInstrRecord *rec = (MemInstrRecord*)(buf+offset);
  rec->setTid(0);   // Must be changed after the multithreaded KLEE is done.
  rec->setInstrId(0);
  rec->setAddr(0);
  offset += KLEE_RECORD_SIZE;
}

void KleeTraceUtil::recordStore(KInstruction *kInstr, ExecutionState *state) {
  checkOffset();
  MemInstrRecord *rec = (MemInstrRecord*)(buf+offset);
  rec->setTid(0);   // Must be changed after the multithreaded KLEE is done.
  rec->setInstrId(0);
  rec->setAddr(0);
  offset += KLEE_RECORD_SIZE;
}

void KleeTraceUtil::recordNonMem(KInstruction *kInstr, ExecutionState *state) {
  checkOffset();
  InstrRecord *rec = (InstrRecord*)(buf+offset);
  rec->setTid(0);   // Must be changed after the multithreaded KLEE is done.
  rec->setInstrId(0);
  offset += KLEE_RECORD_SIZE;
}

void KleeTraceUtil::mapTrace(const char *tracePath) {
  fd = open(tracePath, O_RDWR|O_CREAT, 0644);
  assert(fd >= 0 && "can't open log file!");
  ftruncate(fd, TRACE_MAX_LEN);
  buf = (char*)mmap(0, TRACE_MAX_LEN, PROT_WRITE|PROT_READ,
    MAP_SHARED, fd, 0);
  assert(buf != MAP_FAILED && "can't map log file using mmap()!");
  offset = 0;
}

void KleeTraceUtil::upmapTrace(const char *tracePath) {
  assert(buf);
  munmap(buf, TRACE_MAX_LEN);
  buf = NULL;
  offset = 0;
  assert(fd >= 0);
  ftruncate(fd, offset);
  close(fd);
  fd = -1;
}

void KleeTraceUtil::checkOffset() {
  assert(offset < TRACE_MAX_LEN);
}

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


