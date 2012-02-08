#include "stat.h"
#include "dyn-instrs.h"
#include "instr-id-mgr.h"
#include "callstack-mgr.h"
using namespace tern;

using namespace llvm;

Stat::Stat() {
  interSlicingTime = 0;
  intraSlicingTime = 0;
}

Stat::~Stat() {

}

void Stat::init(InstrIdMgr *idMgr, CallStackMgr *ctxMgr) {
  this->idMgr = idMgr;
  this->ctxMgr = ctxMgr;
}

void Stat::printStat(const char *tag) {
  // TBD.
}

const char *Stat::printInstr(const llvm::Instruction *instr) {
  long addr = (long)instr;
  if (HM_IN(addr, buf)) {
    return buf[addr]->str().c_str();
  } else {
    std::string *str = new std::string;
    llvm::raw_string_ostream *newStream = new llvm::raw_string_ostream(*str);
    instr->print(*newStream);
    buf[addr] = newStream;
    newStream->flush();
    return str->c_str();
  }
}
    
const char *Stat::printValue(const llvm::Value *v) {
  return printInstr((const llvm::Instruction *)v);
}

void Stat::printDynInstr(DynInstr *dynInstr, const char *tag) {
  //fprintf(stderr, "Stat::printDynInstr %d\n", DBG);
  if (DBG) {
    errs() << "\n"
      << "DynInstr {" << tag
      << "} IDX: " << dynInstr->getIndex()
      << ": TID: " << (int)dynInstr->getTid()
      << ": INSTRID: " << dynInstr->getOrigInstrId()
      << ": TAKEN: " << dynInstr->takenReason()
      << ": INSTR: " << printInstr(idMgr->getOrigInstr(dynInstr))
      << "\n";
  }
}


