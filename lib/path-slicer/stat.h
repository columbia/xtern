#ifndef __TERN_PATH_SLICER_STAT_H
#define __TERN_PATH_SLICER_STAT_H

#include <string>

#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Module.h"

#include "macros.h"

namespace tern {
  class DynInstr;
  class DynMemInstr;
  class DynCallInstr;
  class InstrIdMgr;
  class CallStackMgr;
  class FuncSumm;
  class AliasMgr;
  class Stat;
  
  class SymQueryRec {
  public:
    void *pathId;
    DynInstr *dynInstr;
    struct timeval querySt;
    struct timeval queryEnd;
    double queryTime;
    int result;
    SymQueryRec(void *pathId, DynInstr *dynInstr);
    ~SymQueryRec();
  };
  
  class MemOpStat {
  public:
    Stat *stat;
    AliasMgr *aliasMgr;
    InstrIdMgr *idMgr;
    llvm::DenseSet<llvm::Value *> importantValues;
    llvm::DenseSet<DynMemInstr *> mayAliasSymMemInstrs;
    std::string arrayName;
    size_t numLoads;
    size_t numStores;
    size_t numSymLoads;
    size_t numSymStores;
    size_t numSymAliasLoads;
    size_t numSymAliasStores;

    MemOpStat();
    ~MemOpStat();
    void init(AliasMgr *aliasMgr, InstrIdMgr *idMgr, Stat *stat);
    void initArrayName(std::string arrayName);
    void collectImportantValues(DynInstr *dynInstr);
    void collectMemOpStat(DynMemInstr *memInstr);
    void print();
  };
  
  class Stat {
  private:
    std::vector<SymQueryRec *> symQueryStat;
    MemOpStat memOpStat;
    InstrIdMgr *idMgr;
    CallStackMgr *ctxMgr;
    FuncSumm *funcSumm;
    llvm::DenseMap<const llvm::Instruction *, llvm::raw_string_ostream * > instrCache;
    llvm::DenseMap<const llvm::Value *, llvm::raw_string_ostream * > valueCache;
    llvm::Module *origModule;

    /* The set of static instructions and executed instructions in a module. */
    llvm::DenseSet<const llvm::Instruction *> staticInstrs;
    llvm::DenseSet<const llvm::Instruction *> exedStaticInstrs;

    /* The set of static external function calls. */
    llvm::DenseSet<const llvm::Instruction *> externalCalls;

    /* The set of event calls of static instructions. */
    llvm::DenseSet<const llvm::Instruction *> eventCalls;

    /* Path exploration branches frequency, map from instruction to frequency. */
    llvm::DenseMap<llvm::Instruction *, int> pathFreq;

  protected:
    void collectExternalCalls(DynCallInstr *dynCallInstr);
    void collectExedStaticInstrs(DynInstr *dynInstr);
    void collectExedEventCalls(DynCallInstr *dynCallInstr);
    void printFileLoc(llvm::raw_ostream &S, const llvm::Instruction *instr);

  public:
    bool finished;
    long numPrunedStates;
    long numStates;
    unsigned numInstrs;
    unsigned numNotPrunedInternalInstrs;
    unsigned numNotPrunedInstrs;
    unsigned numCoveredInstrs;
    unsigned numUnCoveredInstrs;
    unsigned numPaths;
    unsigned numTests;
    unsigned numChkrErrs;

    struct timeval initSt;
    struct timeval initEnd;
    double initTime;

    struct timeval pathSlicerSt;
    struct timeval pathSlicerEnd;
    double pathSlicerTime;
    
    struct timeval interSlicingSt;
    struct timeval interSlicingEnd;
    double interSlicingTime;

    struct timeval intraSlicingSt;
    struct timeval intraSlicingEnd;
    double intraSlicingTime;

    struct timeval intraChkTgtSt;
    struct timeval intraChkTgtEnd;
    double intraChkTgtTime;

    struct timeval intraPhiSt;
    struct timeval intraPhiEnd;
    double intraPhiTime;

    struct timeval intraBrSt;
    struct timeval intraBrEnd;
    double intraBrTime;

    struct timeval intraRetSt;
    struct timeval intraRetEnd;
    double intraRetTime;

    struct timeval intraCallSt;
    struct timeval intraCallEnd;
    double intraCallTime;

    struct timeval intraMemSt;
    struct timeval intraMemEnd;
    double intraMemTime;

    struct timeval intraNonMemSt;
    struct timeval intraNonMemEnd;
    double intraNonMemTime;

    struct timeval intraBrDomSt;
    struct timeval intraBrDomEnd;
    double intraBrDomTime;

    struct timeval intraBrEvBetSt;
    struct timeval intraBrEvBetEnd;
    double intraBrEvBetTime;

    struct timeval intraBrWrBetSt;
    struct timeval intraBrWrBetEnd;
    double intraBrWrBetTime;

    struct timeval intraBrWrBetGetSummSt;
    struct timeval intraBrWrBetGetSummEnd;
    double intraBrWrBetGetSummTime;    

    struct timeval intraBrWrBetGetBddSt;
    struct timeval intraBrWrBetGetBddEnd;
    double intraBrWrBetGetBddTime;       

    struct timeval intraLiveLoadMemSt;
    struct timeval intraLiveLoadMemEnd;
    double intraLiveLoadMemTime;   

    struct timeval mayAliasSt;
    struct timeval mayAliasEnd;
    double mayAliasTime;   

    struct timeval pointeeSt;
    struct timeval pointeeEnd;
    double pointeeTime;  

    struct timeval getExtCallMemSt;
    struct timeval getExtCallMemEnd;
    double getExtCallMemTime;  

    Stat();
    ~Stat();
    void init(InstrIdMgr *idMgr, CallStackMgr *ctxMgr, FuncSumm *funcSumm, AliasMgr *aliasMgr);
    void printStat(const char *tag);
    const char *printInstr(const llvm::Instruction *instr, bool withFileLoc = false);
    const char *printValue(const llvm::Value *v);
    void printDynInstr(DynInstr *dynInstr, const char *tag,
      bool withFileLoc = false);
    void printDynInstr(llvm::raw_ostream &S, DynInstr *dynInstr, const char *tag,
      bool withFileLoc = false);

    /* Collect statistics on the set of all static instructions and 
    the set of all executed instructions. */
    void collectStaticInstrs(llvm::Module &M);
    size_t sizeOfStaticInstrs();
    size_t sizeOfExedStaticInstrs();

    /* For collecting external calls (for reference of function summary). */
    void printExternalCalls();

    /* Collect stat of executed instrutions. */
    void collectExed(DynInstr *dynInstr);

    /* Utility for explored path frequency. */
    void collectExplored(llvm::Instruction *instr);
    void printExplored();

    void printModule(std::string outputDir);

    void getKLEEFinalStat(unsigned numInstrs, unsigned numCoveredInstrs,
      unsigned numUnCoveredInstrs, unsigned numPaths, unsigned numTests, bool finished);

    void printFinalFormatResults();

    void addNotPrunedInternalInstrs(unsigned traceSize);

    void incNotPrunedStatesInstrs();

    void printEventCalls();

    void incNumChkrErrs();

    void startSymQuery(void *pathId, DynInstr *dynInstr);

    void endSymQuery(void *pathId, DynInstr *dynInstr, int result);

    void printSymQueryStat();
  };
}

#endif

