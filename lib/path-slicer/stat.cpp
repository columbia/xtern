#include <unistd.h>

#include "util.h"
#include "stat.h"
#include "dyn-instrs.h"
#include "instr-id-mgr.h"
#include "callstack-mgr.h"
#include "func-summ.h"
using namespace tern;

#include "llvm/Metadata.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;

#include "klee/Statistic.h"
using namespace klee;

extern cl::opt<std::string> UseOneChecker;
extern cl::opt<bool> MarkPrunedOnly;

MemOpStat::MemOpStat() {
  aliasMgr = NULL;
  idMgr = NULL;
  stat = NULL;
  arrayName = "";
  numLoads = 0;
  numStores = 0;
  numSymLoads = 0;
  numSymStores = 0;
  numSymAliasLoads = 0;
  numSymAliasStores = 0;
}

MemOpStat::~MemOpStat() {

}

void MemOpStat::init(AliasMgr *aliasMgr, InstrIdMgr *idMgr, Stat *stat) {
  this->aliasMgr = aliasMgr;
  this->idMgr = idMgr;
  this->stat = stat;
}

void MemOpStat::initArrayName(std::string arrayName) {
  this->arrayName = arrayName;
}

void MemOpStat::collectImportantValues(DynInstr *dynInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynInstr);
  if (Util::isGetElemPtr(instr)) {
    Value *ptrOprd = instr->getOperand(0);
    if (ptrOprd->hasName() && ptrOprd->getNameStr() == arrayName) {
      importantValues.insert((Value *)instr);
      stat->printDynInstr(dynInstr, "MemOpStat::collectImportantValues");
    }
  }
}

void MemOpStat::collectMemOpStat(DynMemInstr *memInstr) {
  Instruction *instr = idMgr->getOrigInstr((DynInstr *)memInstr);
  assert(Util::isMem(instr));
  Value *ptr = NULL;
  bool isSym = false;
  
  if (Util::isLoad(instr)) {
    ptr = instr->getOperand(0);
    numLoads++;
    if (memInstr->isAddrSymbolic()) {
      isSym = true;
      numSymLoads++;
    }
  } else {
    assert(Util::isStore(instr));
    ptr = instr->getOperand(1);
    numStores++;
    if (memInstr->isAddrSymbolic()) {
      isSym = true;
      numSymStores++;
    }
  }

  // Alias query.
  if (isSym) {
    DenseSet<Value *>::iterator itr(importantValues.begin());
    for (; itr != importantValues.end(); ++itr) {
      Value *v = *itr;
      if (v == ptr || aliasMgr->mayAlias(v, ptr)) {
        mayAliasSymMemInstrs.insert(memInstr);
        if (Util::isLoad(instr))
          numSymAliasLoads++;
        else {
          assert(Util::isStore(instr));
          numSymAliasStores++;
        }
        return;
      }
    }
  }
}

void MemOpStat::print() {
  errs() << "MemOpStat::print"
    << ": arrayName: " << arrayName
    << ": [numLoads: " << numLoads
    << ": numSymLoads: " << numSymLoads
    << ": numSymAliasLoads: " << numSymAliasLoads
    << "]"
    << ":    [numStores: " << numStores
    << ": numSymStores: " << numSymStores
    << ": numSymAliasStores: " << numSymAliasStores
    << "]\n";

  errs() << "MemOpStat::print importantValues size " << importantValues.size() << "\n";
  DenseSet<Value *>::iterator itr(importantValues.begin());
  int i = 0;
  for (; itr != importantValues.end(); ++itr) {
    Instruction *instr = dyn_cast<Instruction>(*itr);
    assert(instr);
    errs() << "MemOpStat::print importantValues [" << i << "]: " <<  stat->printInstr(instr) << "\n";
    i++;
  }

  DenseSet<DynMemInstr *>::iterator dynItr(mayAliasSymMemInstrs.begin());
  i = 0;
  for (; dynItr != mayAliasSymMemInstrs.end(); ++dynItr) {
    errs() << "MemOpStat::print mayAliasSymMemInstrs [" << i << "]: ";
    stat->printDynInstr((DynInstr *)(*dynItr), "MemOpStat::print");
    i++;
  }
}

Stat::Stat() {
  finished = false;
  numPrunedStates = 0;
  numStates = 0;
  numInstrs = 0;
  numNotPrunedInternalInstrs = 0;
  numNotPrunedInstrs = 0;
  numCoveredInstrs = 0;
  numUnCoveredInstrs = 0;
  numPaths = 0;
  numTests = 0;
  numChkrErrs = 0;

  initTime = 0;
  pathSlicerTime = 0;
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
  intraBrWrBetGetSummTime = 0;
  intraBrWrBetGetBddTime = 0;
  intraLiveLoadMemTime = 0;
  mayAliasTime = 0;
  pointeeTime = 0;
  getExtCallMemTime = 0;
}

Stat::~Stat() {

}

void Stat::init(InstrIdMgr *idMgr, CallStackMgr *ctxMgr, FuncSumm *funcSumm, AliasMgr *aliasMgr) {
  this->idMgr = idMgr;
  this->ctxMgr = ctxMgr;
  this->funcSumm = funcSumm;
  //memOpStat.init(aliasMgr, idMgr, this);
  //memOpStat.initArrayName("newArgv"); // Currently only looks at argv, can look at other arrays as well.
}

void Stat::printStat(const char *tag) {
  std::string pruneType = MarkPrunedOnly?"Mark":"Real";
  errs() << "\n\n" << tag << ": "
    << "app: " << origModule->getModuleIdentifier() << ", "
    << "chercker: " << UseOneChecker << ", "
    << "pruneType: " << pruneType << ", "
    << "time: " << (unsigned long)time(NULL) << ", "
    << "processId: " << getpid() << ", "
    << "intraSlicingTime: " << intraSlicingTime << ", "
    << "intraChkTgtTime: " << intraChkTgtTime << ", "
    << "intraPhiTime: " << intraPhiTime << ", "
    << "intraBrTime: " << intraBrTime << " ("
      << intraBrDomTime << ", " << intraBrEvBetTime << ", " << intraBrWrBetTime
        << " (intraBrWrBetGetSummTime: " << intraBrWrBetGetSummTime 
        << ", intraBrWrBetGetBddTime: " << intraBrWrBetGetBddTime
        << ") "
  	  << "), "
    << "intraLiveLoadMemTime: " << intraLiveLoadMemTime << ", "
    << "mayAliasTime: " << mayAliasTime << ", "
    << "pointeeTime: " << pointeeTime << ", "
    << "getExtCallMemTime: " << getExtCallMemTime << ", "
    << "intraRetTime: " << intraRetTime << ", "
    << "intraCallTime: " << intraCallTime << ", "
    << "intraMemTime: " << intraMemTime << ", "
    << "intraNonMemTime: " << intraNonMemTime << ", "
    << "StaticExed/Static Instrs: " << sizeOfExedStaticInstrs() << "/" << sizeOfStaticInstrs() << ", "
    << "numPrunedStates/numStates: " << numPrunedStates << "/" << numStates << ", "    
    // TBD.
    << "\n";

  //memOpStat.print();
}

void Stat::printFinalFormatResults() {
  // Print FORMAT.
  std::string pruneType = MarkPrunedOnly?"Mark":"Real";
  std::string finishResult = finished?"Yes":"No";
  std::string keyTag = "*";
  std::cerr << "\n\n" << "FORMAT ITEMS:    "
    << "|| App || Checker || Mark/Real prune || Finished || All time (sec) || Path slicer time "
    << "|| Init time || Pruned states || All states (paths) "
    << "|| \\# Tests || \\# Instrs exed || \\# Not pruned Instrs exed || \\# Not pruned internal Instrs exed "
    << "|| \\# Static Instrs exed || \\# Static Instrs || \\# Static exed events || \\# Static events (no fp) || \\# Chkr errs || Note ||\n"
    << "FORMAT RESULTS:    ";
  std::cerr << "| " << origModule->getModuleIdentifier();
  std::cerr << " | " << UseOneChecker;
  std::cerr << " | " << pruneType;
  std::cerr << " | " << keyTag << finishResult << keyTag;
  std::cerr << " | " << std::dec << pathSlicerTime;
  std::cerr << " | " << std::dec << intraSlicingTime;
  std::cerr << " | " << std::dec << initTime;
  std::cerr << " | " << std::dec << numPrunedStates;
  std::cerr << " | " << std::dec << numStates;
  std::cerr << " | " << std::dec << numTests;
  std::cerr << " | " << std::dec << numInstrs;
  std::cerr << " | " << keyTag; std::cerr << std::dec << numNotPrunedInstrs; std::cerr << keyTag;
  std::cerr << " | " << std::dec << numNotPrunedInternalInstrs;
  std::cerr << " | " << std::dec << sizeOfExedStaticInstrs();
  std::cerr << " | " << std::dec << sizeOfStaticInstrs();
  std::cerr << " | " << std::dec << eventCalls.size();
  std::cerr << " | " << std::dec << funcSumm->numEventCallSites();
  std::cerr << " | " << std::dec << numChkrErrs;
  std::cerr << " | " << "tbd";
  //TBA
  
  std::cerr  << " | "
  	<< "\n\n\n";

}

const char *Stat::printInstr(const llvm::Instruction *instr, bool withFileLoc) {
  if (DM_IN(instr, buf)) {
    return buf[instr]->str().c_str();
  } else {
    std::string *str = new std::string;
    llvm::raw_string_ostream *newStream = new llvm::raw_string_ostream(*str);
    //if (withFileLoc)
      //printFileLoc(*newStream, instr);
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

  // Print the recorded real called function (including resolving of function pointers).
  if (DBG && Util::isCall(instr)) {
    DynCallInstr *call = (DynCallInstr *)dynInstr;
    Function *f = call->getCalledFunc();
    fprintf(stderr, "REAL CALLED: %s\n\n", f?f->getNameStr().c_str():"NIL");
  }

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
  this->origModule = &M;
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

void Stat::collectEventCalls(DynCallInstr *dynCallInstr) {
  Instruction *instr = idMgr->getOrigInstr(dynCallInstr);
  eventCalls.insert(instr);
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
  if (Util::isCall(instr)) {
    DynCallInstr *callInstr = (DynCallInstr *)dynInstr;
    if (!funcSumm->isInternalCall(dynInstr))
      collectExternalCalls(callInstr);
    if (funcSumm->isEventCall(callInstr))
      collectEventCalls(callInstr);
  }

  // Collect mem op stat.
  /*memOpStat.collectImportantValues(dynInstr);
  if (Util::isMem(instr))
    memOpStat.collectMemOpStat((DynMemInstr *)dynInstr);*/
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

void Stat::printModule(std::string outputDir) {
  static const char *moduleFile = "module.stat";
  static const char *instrSep = "        ";
  static const char *coveredTag = "[COVERED]";
  static const char *uncoveredTag = "[UN-COVERED]";
  
  char path[BUF_SIZE];
  memset(path, 0, BUF_SIZE);
  snprintf(path, BUF_SIZE, "%s/%s", outputDir.c_str(), moduleFile);
  std::string ErrorInfo;
  raw_fd_ostream OS(path, ErrorInfo, raw_fd_ostream::F_Append);
  
  for (Module::iterator f = origModule->begin(); f != origModule->end(); ++f) {
    if (!funcSumm->isInternalFunction(f))
      continue;
    unsigned numInstrs = 0;
    unsigned numCoveredInstrs = 0;
    OS << "\nFunction: " << f->getNameStr() << "(...) {\n";
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      OS << "BB: " << b->getNameStr() << ":\n";
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (Util::isIntrinsicCall(i)) // Ignore intrinsic calls.
          continue;
        if (idMgr->getOrigInstrId(i) != -1) {
          numInstrs++;
          if (DBG)
            OS << printInstr(i) << instrSep;
          if (DM_IN(i, exedStaticInstrs)) {
            numCoveredInstrs++;
            if (DBG)
              OS << coveredTag;
          }
          if (DBG)
            OS << "\n";
        }
      }
      if (!DBG) {
        BasicBlock::iterator i = b->begin();
        if (DM_IN(i, exedStaticInstrs))
          OS << "  " << coveredTag << "\n";
        else
          OS << "  " << uncoveredTag << "\n";
      }
      OS << "\n";
    }
    OS << "} " << f->getNameStr() << " " << coveredTag 
      << " " << numCoveredInstrs << "/" << numInstrs << "\n\n";
  }

  OS.flush();
  OS.close();
}

void Stat::getKLEEFinalStat(unsigned numInstrs, unsigned numCoveredInstrs,
      unsigned numUnCoveredInstrs, unsigned numPaths, unsigned numTests, bool finished) {
  this->numInstrs = numInstrs;
  this->numCoveredInstrs = numCoveredInstrs;
  this->numUnCoveredInstrs = numUnCoveredInstrs;
  this->numPaths = numPaths;
  this->numTests = numTests;
  this->finished = finished;
}

void Stat::addNotPrunedInternalInstrs(unsigned traceSize) {
  numNotPrunedInternalInstrs += traceSize;
}

void Stat::incNotPrunedStatesInstrs() {
  numNotPrunedInstrs++;
}

void Stat::printEventCalls() {
  // Print exed event calls.
  errs() << BAN;
  DenseSet<const Instruction *>::iterator itr(eventCalls.begin());
  for (; itr != eventCalls.end(); ++itr) {
    errs() << "Stat::printEventCalls exed " << Util::printNearByFileLoc(*itr) 
      << ": " << printInstr(*itr) << "\n\n";
  }
  errs() << BAN;
  
  // Print all static event calls.
  funcSumm->printEventCalls();
  errs() << BAN;
}

void Stat::incNumChkrErrs() {
  numChkrErrs++;
}

