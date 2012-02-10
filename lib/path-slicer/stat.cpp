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
  errs() << "Stat::printInstr: " << (void *)(instr) << "\n";
  if (DM_IN(instr, buf)) {
    return buf[instr]->str().c_str();
  } else {
    std::string *str = new std::string;
    llvm::raw_string_ostream *newStream = new llvm::raw_string_ostream(*str);
    instr->print(*newStream);
    buf[instr] = newStream;
    newStream->flush();
    return str->c_str();
  }
}
    
const char *Stat::printValue(const llvm::Value *v) {
  return printInstr((const llvm::Instruction *)v);
}

void Stat::printDynInstr(DynInstr *dynInstr, const char *tag) {
  fprintf(stderr, "Stat::printDynInstr %p, tid %u\n", (void *)dynInstr, 
    (unsigned)dynInstr->getTid());
  if (DBG) {
    errs() << "\n"
      << "DynInstr {" << tag
      << "} IDX: " << dynInstr->getIndex()
      << ": TID: " << (int)dynInstr->getTid()
      << ": INSTRID: " << dynInstr->getOrigInstrId()
      << ": TAKEN: " << dynInstr->takenReason();
    errs()
      << ": INSTR: " << *(idMgr->getOrigInstr(dynInstr))
      << "\n";
  }
}


