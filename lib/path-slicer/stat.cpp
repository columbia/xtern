#include "util.h"
#include "stat.h"
#include "dyn-instrs.h"
#include "instr-id-mgr.h"
#include "callstack-mgr.h"
#include "func-summ.h"
using namespace tern;

#include "llvm/Metadata.h"
#include "llvm/Analysis/DebugInfo.h"
using namespace llvm;

Stat::Stat() {
  interSlicingTime = 0;
  intraSlicingTime = 0;
  intraChkTgtTime = 0;
  intraPhiTime = 0;
  intraBrTime = 0;
  intraRetTime = 0;
  intraCallTime = 0;
  intraMemTime = 0;
  intraNonMemTime = 0;
  intraBrDomTime = 0;
  intraBrEvBetTime = 0;
  intraBrWrBetTime = 0;
}

Stat::~Stat() {

}

void Stat::init(InstrIdMgr *idMgr, CallStackMgr *ctxMgr, FuncSumm *funcSumm) {
  this->idMgr = idMgr;
  this->ctxMgr = ctxMgr;
  this->funcSumm = funcSumm;
}

void Stat::printStat(const char *tag) {
  errs() << "\n\n" << tag << ": "
    << "intraSlicingTime: " << intraSlicingTime << ", "
    
    << "intraChkTgtTime: " << intraChkTgtTime << ", "
    << "intraPhiTime: " << intraPhiTime << ", "
    << "intraBrTime: " << intraBrTime << " ("
      << intraBrDomTime << ", " << intraBrEvBetTime << ", " << intraBrWrBetTime << "), "
    << "intraRetTime: " << intraRetTime << ", "
    << "intraCallTime: " << intraCallTime << ", "
    << "intraMemTime: " << intraMemTime << ", "
    << "intraNonMemTime: " << intraNonMemTime << ", "
    
    // TBD.
    << "\n";
}

const char *Stat::printInstr(const llvm::Instruction *instr, bool withFileLoc) {
  if (DM_IN(instr, buf)) {
    return buf[instr]->str().c_str();
  } else {
    std::string *str = new std::string;
    llvm::raw_string_ostream *newStream = new llvm::raw_string_ostream(*str);
    if (withFileLoc)
      printFileLoc(*newStream, instr);
    (*newStream) << "F: " << Util::getFunction(instr)->getNameStr()
      << ": BB: " << Util::getBasicBlock(instr)->getNameStr() << ": ";
    instr->print(*newStream);
    buf[instr] = newStream;
    newStream->flush();
    return str->c_str();
  }
}

void Stat::printFileLoc(raw_ostream &S, const Instruction *instr) {
  if (MDNode *N = instr->getMetadata("dbg")) {
    DILocation Loc(N);                      // DILocation is in DebugInfo.h
    unsigned Line = Loc.getLineNumber();
    StringRef File = Loc.getFilename();
    StringRef Dir = Loc.getDirectory();
    S << "Location: " <<Dir << "/" << File << ":" << Line << ": ";
  }
}

void Stat::printDynInstr(DynInstr *dynInstr, const char *tag, bool withFileLoc) {
  printDynInstr(errs(), dynInstr, tag, withFileLoc);
}

void Stat::printDynInstr(raw_ostream &S, DynInstr *dynInstr, const char *tag, bool withFileLoc) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  S << tag
    << ": IDX: " << dynInstr->getIndex()
    << ": TID: " << (int)dynInstr->getTid()
    << ": INSTRID: " << dynInstr->getOrigInstrId()
    << ": TAKEN: " << dynInstr->takenReason()
    << ": INSTR: " << printInstr(instr, withFileLoc)
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

void Stat::collectExplored(llvm::Instruction *instr) {
  if (pathFreq.count(instr) == 0)
    pathFreq[instr] = 1;
  else
    pathFreq[instr] = pathFreq[instr] + 1;
}

void Stat::printExplored() {
  long numInternal = 0;
  long numExternal = 0;
  DenseMapIterator<Instruction *, int> itr(pathFreq.begin());
  for (; itr != pathFreq.end(); itr++) {
    Instruction *instr = itr->first;
    assert(instr);
    Function *f = Util::getFunction(instr);
    BasicBlock *bb = Util::getBasicBlock(instr);
    std::string property = (funcSumm->isInternalFunction(f))?"INTERNAL":"EXTERNAL";
    errs() << "Stat::printExplored: F (" << property << "): " << f->getNameStr()
      << ": BB: " << bb->getNameStr()
      << ": EXPLORED FREQ: " << itr->second << "\n";
    if (funcSumm->isInternalFunction(f))
      numInternal += itr->second;
    else
      numExternal += itr->second;      
  }
  errs() << "TOTAL EXPLORED FREQ: " << "(INTERNAL: " << numInternal
    << ", EXTERNAL: " << numExternal << ")" << "\n";
}


