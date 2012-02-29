#include "util.h"
#include "stat.h"
#include "dyn-instrs.h"
#include "instr-id-mgr.h"
#include "callstack-mgr.h"
#include "func-summ.h"
using namespace tern;

using namespace llvm;

Stat::Stat() {
  interSlicingTime = 0;
  intraSlicingTime = 0;
}

Stat::~Stat() {

}

void Stat::init(InstrIdMgr *idMgr, CallStackMgr *ctxMgr, FuncSumm *funcSumm) {
  this->idMgr = idMgr;
  this->ctxMgr = ctxMgr;
  this->funcSumm = funcSumm;
}

void Stat::printStat(const char *tag) {
  // TBD.
}

const char *Stat::printInstr(const llvm::Instruction *instr) {
  if (DM_IN(instr, buf)) {
    return buf[instr]->str().c_str();
  } else {
    std::string *str = new std::string;
    llvm::raw_string_ostream *newStream = new llvm::raw_string_ostream(*str);
    (*newStream) << "F: " << Util::getFunction(instr)->getNameStr()
      << ": BB: " << Util::getBasicBlock(instr)->getNameStr() << ": ";
    instr->print(*newStream);
    buf[instr] = newStream;
    newStream->flush();
    return str->c_str();
  }
}

void Stat::printDynInstr(DynInstr *dynInstr, const char *tag) {
  printDynInstr(errs(), dynInstr, tag);
}

void Stat::printDynInstr(raw_ostream &S, DynInstr *dynInstr, const char *tag) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  S << tag
    << ": IDX: " << dynInstr->getIndex()
    << ": TID: " << (int)dynInstr->getTid()
    << ": INSTRID: " << dynInstr->getOrigInstrId()
    << ": TAKEN: " << dynInstr->takenReason()
    << ": INSTR: " << printInstr(instr)
    << "\n\n";

  // Print the condition if this is a symbolic branch.
  if (DBG && Util::isBr(instr) && !Util::isUniCondBr(instr)) {
    DynBrInstr *br = (DynBrInstr *)dynInstr;
    if (br->isSymbolicBr()) {
      errs() << EXPR_BEGIN;
      br->getBrCondition()->print(std::cerr);
      errs() << EXPR_END;
    }
  }
}

void Stat::collectStaticInstrs(llvm::Module &M) {
  for (Module::iterator f = M.begin(); f != M.end(); ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (Util::isIntrinsicCall(i)) // Ignore intrinsic calls.
          continue;
        staticInstrs.insert(i);
      }
}

void Stat::collectExedStaticInstrs(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isIntrinsicCall(instr)) // Ignore intrinsic calls.
    return;
  exedStaticInstrs.insert(instr);
}

size_t Stat::sizeOfStaticInstrs() {
  return staticInstrs.size();
}

size_t Stat::sizeOfExedStaticInstrs() {
  return exedStaticInstrs.size();
}

void Stat::collectExternalCalls(DynCallInstr *dynCallInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynCallInstr);
  externalCalls.insert(instr);
}

void Stat::printExternalCalls() {
  DenseSet<const Instruction *>::iterator itr(externalCalls.begin());
  errs() << "\n";
  for (; itr != externalCalls.end(); ++itr)
    errs() << "External call: " << printInstr(*itr) << "\n";
  errs() << "\n";
}

void Stat::collectExed(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  collectExedStaticInstrs(dynInstr);
  if (Util::isCall(instr) && !funcSumm->isInternalCall(dynInstr))
    collectExternalCalls((DynCallInstr *)dynInstr);
}

