#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CallSite.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
using namespace llvm;

#include "bc2bdd/Bc2Bdd.h"
#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "util.h"
#include "path-slicer.h"
using namespace tern;

using namespace klee;

extern cl::opt<bool> MarkPrunedOnly;

static RegisterPass<PathSlicer> X(
		"tern-path-slicer",
		"Tern Path Slicer",
		false,
		true); // is analysis

/* Only need to passed in max sliced bc and simplified bc. Orig bc is passed 
in using the default LLVM input. */
static cl::opt<string> MXSlicedBC(
		"mx-sliced-bc",
		cl::desc("The max sliced bc file path"),
		cl::init(""));

static cl::opt<string> SimplifiedBC(
		"simplified-bc",
		cl::desc("The simplified bc file path"),
		cl::init(""));

/* The path of landmark trace. This is used in instruction id manager. */
static cl::opt<string> LmTrace(
		"landmark-trace",
		cl::desc("The landmark trace"),
		cl::init(""));


/* The full execution trace. */
static cl::opt<string> FullTrace(
		"full-trace",
		cl::desc("The full trace"),
		cl::init(""));

/* The ID starting point of a schedule to merge branch conditions and order 
constraints. */
static cl::opt<string> SchedLeaf(
		"sched-leaf",
		cl::desc("The schedule leaf"),
		cl::init(""));

char PathSlicer::ID = 0;

PathSlicer::PathSlicer(): ModulePass(&ID) {
  startRecord = false;
  isKleeHalted = false;
  fprintf(stderr, "PathSlicer::PathSlicer()\n");
  load_options("local.options");
}

PathSlicer::~PathSlicer() {
  ENDTIME(stat.pathSlicerTime, stat.pathSlicerSt, stat.pathSlicerEnd);
  if (DBG)
    stat.printModule(this->outputDir);
  stat.printStat("PathSlicer::calStat FINAL");
  stat.printEventCalls();
  stat.printFinalFormatResults();
  if (DBG)
    fprintf(stderr, "PathSlicer::~PathSlicer()\n");
}

void PathSlicer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

bool PathSlicer::runOnModule(Module &M) {
  BEGINTIME(stat.pathSlicerSt);
  init(M);
  return false;
}

void PathSlicer::init(llvm::Module &M) {
  fprintf(stderr, "PathSlicer::init begin\n");
  BEGINTIME(stat.initSt);
  Module *origModule = &M;
  Module *mxModule = NULL;
  Module *simModule = NULL;

  if (NORMAL_SLICING) {
    // NOP.
  } else if (MAX_SLICING) {
    mxModule = loadModule(MXSlicedBC.c_str());
    assert(mxModule && "Argument -mx-sliced-bc should be valid.");
  } else if (RANGE_SLICING) {
    mxModule = loadModule(MXSlicedBC.c_str());
    assert(mxModule && "Argument -mx-sliced-bc should be valid.");
    simModule = loadModule(SimplifiedBC.c_str());
    assert(simModule && "Argument -simplified-bc should be valid.");
  } else {
    assert(false && "Slicing mode should be valid.");
  }

  /* Init call graph. */
  PassManager *cgPM = new PassManager;
  Util::addTargetDataToPM(origModule, cgPM);
  cgPM->add(&CG);
  cgPM->run(*origModule);

  /* Init event manager. */
  evMgr.initCallGraph(&CG);
  PassManager *eventPM = new PassManager;
  Util::addTargetDataToPM(origModule, eventPM);
  eventPM->add(&evMgr);
  eventPM->run(*origModule);
  
  /* Init function summary. */
  funcSumm.init(&idMgr, &evMgr);
  PassManager *funcPM = new PassManager;
  Util::addTargetDataToPM(origModule, funcPM);
  funcPM->add(&funcSumm);
  funcPM->run(*origModule);

  /* Init oprd summary. */
  oprdSumm.init(&stat, &funcSumm, &aliasMgr, &idMgr, &CG);
  PassManager *oprdPM = new PassManager;
  if (NORMAL_SLICING) {
    Util::addTargetDataToPM(origModule, oprdPM);
    oprdPM->add(&oprdSumm);
    oprdPM->run(*origModule);
  } else if (MAX_SLICING) {
    Util::addTargetDataToPM(mxModule, oprdPM);
    oprdPM->add(&oprdSumm);
    oprdPM->run(*mxModule);
  } else {
    Util::addTargetDataToPM(simModule, oprdPM);
    oprdPM->add(&oprdSumm);
    oprdPM->run(*simModule);
    assert(false);// TBD. NOT SURE WHETHER SHOULD PASS IN MX OR SIM MODULE HERE.
  }

  /* Init instruction id manager. */
  idMgr.init(&stat, &funcSumm);
  idMgr.initModules(origModule, mxModule, simModule, LmTrace.c_str());

  /* Init call stack manager. */
  ctxMgr.init(&stat, &idMgr, &funcSumm, &tgtMgr);

  /* Init CFG manager. */
  cfgMgr.initStat(&stat);
  PassManager *cfgPM = new PassManager;
  Util::addTargetDataToPM(origModule, cfgPM);
  cfgPM->add(&cfgMgr);
  cfgPM->run(*origModule);

  /* Init alias manager. */
  aliasMgr.initStat(&stat);
  aliasMgr.initInstrIdMgr(&idMgr);
  aliasMgr.initBAA(&(CG.getAnalysis<BddAliasAnalysis>()));

  /* Init trace util. */
  if (KLEE_RECORDING) {
    traceUtil = new KleeTraceUtil();
    ((KleeTraceUtil *)traceUtil)->init(&idMgr, &stat);
    traceUtil->initCallStackMgr(&ctxMgr);
  } else if (XTERN_RECORDING) {
    traceUtil = new XTernTraceUtil();
    traceUtil->initCallStackMgr(&ctxMgr);
  }
  else
    assert(false);

  /* Init stat. */
  stat.init(&idMgr, &ctxMgr, &funcSumm, &aliasMgr);
  stat.collectStaticInstrs(*origModule);

  /* Destroy call graph, because it is not longer useful. */
  CG.destroy();

  ENDTIME(stat.initTime, stat.initSt, stat.initEnd);
  fprintf(stderr, "PathSlicer::init end\n");
}

Module *PathSlicer::loadModule(const char *path) {
  string bcPath = path;
  string ErrorMsg;
  Module *module = NULL;
  MemoryBuffer *buffer = MemoryBuffer::getFileOrSTDIN(bcPath, &ErrorMsg);
  if (buffer) {
    module = getLazyBitcodeModule(buffer, getGlobalContext(), &ErrorMsg);
    assert(module);
    if (module->MaterializeAllPermanently(&ErrorMsg)) {
      assert(false);
    }
  }
  return module;
}

void PathSlicer::enforceRacyEdges() {
  // TBD: enforce all racy edges, and split new regions.
}

void PathSlicer::runPathSlicer(void *pathId, set<size_t> &rmBrs,
  set<size_t> &rmCalls, bool isHalted) {  
  // Collect stat.
  bool isPruned = ((ExecutionState *)pathId)->isPruned;
  
  // Get trace of current path and do some pre-processing.
  if (!DM_IN(pathId, allPathTraces))
    return;
  BEGINTIME(stat.intraSlicingSt);
  DynInstrVector *trace = allPathTraces[pathId];
  fprintf(stderr, "PathSlicer::runPathSlicer START pathId %p, isPruned %d, isHalted %d, size " SZ "\n",
    pathId, isPruned, isHalted, trace->size());
  if (isPruned)
    goto finish;
  if (isHalted) {
    isKleeHalted = true;
    goto finish;
  }
  if (trace->size() == 0)
    goto finish;
  stat.addNotPrunedInternalInstrs((unsigned)trace->size());
  traceUtil->preProcess(trace);

#if 0
  // Enforce racy edges.
  enforceRacyEdges();
  
  // Run inter-slicer.
  interSlicer.detectInputDepRaces(&instrRegions);
#endif

  // Run intra-slicer.
  // TBD: should have a thread-id loops to traverse all thread ids.
  do {
    // (1) Specify current slicing thread id.
    uchar tid = 0;

    // (2) Init slicing start index.
    size_t startIndex = trace->size() - 1;
    
    /* (3) Init slicing sub modules. This function will also clean live set and 
      slice set in the intra-thread slicer. */
    intraSlicer.init((ExecutionState *)pathId, &oprdSumm, &funcSumm,
      &aliasMgr, &idMgr, &cfgMgr, &ctxMgr, &tgtMgr, &stat, trace, startIndex);

    // If it is in testing mode, take the last instruction in trace as test target.
    if (INTRA_SLICING_FOR_TEST)
      intraSlicer.takeTestTarget(trace->back());

    // (5) Do intra-thread slicing.
    intraSlicer.detectInputDepRaces(tid);
  } while (0);
  
  // Calculate stat results.
  calStat(rmBrs, rmCalls);

  // Free the trace along current path. 
  traceUtil->postProcess(trace);

finish:  
  tgtMgr.clearTargets(pathId);
  allPathTraces.erase(pathId);
  delete trace;
  ENDTIME(stat.intraSlicingTime, stat.intraSlicingSt, stat.intraSlicingEnd);
  fprintf(stderr, "PathSlicer::runPathSlicer END pathId %p, isPruned %d, isHalted %d\n",
    pathId, isPruned, isHalted);
}

void PathSlicer::calStat(set<size_t> &rmBrs, set<size_t> &rmCalls) {
  errs() << BAN;
  interSlicer.calStat();
  intraSlicer.calStat(rmBrs, rmCalls);
  stat.printStat("PathSlicer::calStat");
  stat.printExplored();
  errs() << BAN;
}

void PathSlicer::initKModule(KModule *kmodule) {
  if (KLEE_RECORDING)
    ((KleeTraceUtil *)traceUtil)->initKModule(kmodule);
  else
    assert(false);
}

void PathSlicer::initOutputDir(std::string dir) {
  fprintf(stderr, "PathSlicer::initOutputDir %s\n", dir.c_str());
  outputDir = dir;
}


void PathSlicer::record(void *pathId, void *instr, void *state, void *f) {
  //fprintf(stderr, "PathSlicer::record enter, pathId %p\n", (void *)pathId);
  //Instruction *staticInstr = ((KInstruction *)instr)->inst;
  //if (staticInstr->getOpcode() == Instruction::Ret || staticInstr->getOpcode() == Instruction::Call)
	  //SERRS << "PathSlicer::record: " << (void *)pathId << " : " << stat.printInstr(staticInstr) << "\n";
  if (!getStartRecord(instr))
    return;
  if (!DM_IN(pathId, allPathTraces))
    allPathTraces[pathId] = new DynInstrVector;
  traceUtil->record(allPathTraces[pathId], instr, state, f);
  //fprintf(stderr, "PathSlicer::record finished, pathId %p\n", (void *)pathId);
}

void PathSlicer::copyTrace(void *newPathId, void *curPathId) {
  //fprintf(stderr, "PathSlicer::copyTrace new %p, cur %p\n", (void *)newPathId, (void *)curPathId);
  assert (!DM_IN(newPathId, allPathTraces));
  if (!DM_IN(curPathId, allPathTraces))
    return;
  allPathTraces[newPathId] = new DynInstrVector;
  DynInstrVector *newTrace = allPathTraces[newPathId];
  DynInstrVector *curTrace = allPathTraces[curPathId];
  newTrace->insert(newTrace->begin(), curTrace->begin(), curTrace->end());
  tgtMgr.copyTargets(newPathId, curPathId);
}

/* The assertions in this function indicate we only support one checker at one run. */
void PathSlicer::recordCheckerResult(void *pathId, Checker::Result globalResult,
    Checker::Result localResult, unsigned numTests) {
  bool isPruned = ((ExecutionState *)pathId)->isPruned;
  // DBG.
  if (BIT_NEQ(globalResult, Checker::OK) || BIT_NEQ(localResult, Checker::OK)) {
    fprintf(stderr, "PathSlicer::recordCheckerResult, pathId %p, isPruned %d, isHalted %d, globalResult %d, localResult %d.\n",
      (void *)pathId, isPruned, isKleeHalted, (int)globalResult, (int)localResult);
    if (MarkPrunedOnly && isPruned && (globalResult & Checker::ERROR || localResult & Checker::ERROR)) {
      fprintf(stderr, "PathSlicer::recordCheckerResult, a state %p triggered a checker error after pruned.\n",
        pathId);
      //exit(1);
    }
  }

  // If this state is already pruned, just discard it, since its trace and targets have already been freed.  
  if (isPruned)
    return;
  
  if (BIT_NEQ(globalResult, Checker::OK) || BIT_NEQ(localResult, Checker::OK)) {
    DynInstrVector *trace = allPathTraces[pathId];
    assert(trace);
    assert(trace->size() > 0);
    DynInstr *dynInstr = trace->back();
    if (globalResult & Checker::IMPORTANT || localResult & Checker::IMPORTANT) {
      assert(BIT_NEQ(globalResult, Checker::ERROR) && BIT_NEQ(localResult, Checker::ERROR));
      tgtMgr.markTarget(pathId, dynInstr, TakenFlags::CHECKER_IMPORTANT, (klee::Checker::ResultType)localResult);
      if (DBG)
        stat.printDynInstr(dynInstr, "PathSlicer::recordCheckerResult Checker::IMPORTANT");
    } else {
      assert(BIT_NEQ(globalResult, Checker::IMPORTANT) && BIT_NEQ(localResult, Checker::IMPORTANT));
      tgtMgr.markTarget(pathId, dynInstr, TakenFlags::CHECKER_ERROR, (klee::Checker::ResultType)localResult);
      if (DBG && !isKleeHalted)  // Output the trace is klee is not halted. This will not affect checker reports.
        traceUtil->store(numTests+1, outputDir.c_str(), trace);
      if (DBG)
        stat.printDynInstr(dynInstr, "PathSlicer::recordCheckerResult Checker::ERROR");
      stat.incNumChkrErrs();
    }
  }
}

bool PathSlicer::getStartRecord(void *instr) {
  if (startRecord)
    return true;
  else {
    KInstruction *kInstr = (KInstruction *)instr;
    if (cfgMgr.getFirstInstr() == kInstr->inst) {
      startRecord = true;
      SERRS << "PathSlicer::getStartRecord: " << stat.printInstr(kInstr->inst) << "\n";
      return true;
    }
  }
  return false;
}

bool PathSlicer::isInternalInstr(llvm::Instruction *instr) {
  return (idMgr.getOrigInstrId(instr) != -1);
}

void PathSlicer::dumpTrace(DynInstrVector *trace, const char *tag) {
  assert(trace);
  errs() << BAN;
  for (size_t i = 0; i < trace->size(); i++) {
    stat.printDynInstr(trace->at(i), tag);
  }
  errs() << BAN;
}


// Returning -1 is legal, klee states from klee_init_env().
size_t PathSlicer::getLatestBrOrExtCallIdx(void *pathId) {
  if (!DM_IN(pathId, allPathTraces))
    return (size_t)-1;
  DynInstrVector *trace = allPathTraces[pathId];
  if (trace->size() == 0)
    return (size_t)-1;

  /* Note: according to klee, states do not have to be forked by only branches, 
	it could be load/store, malloc()/free(), or calls to function pointers.
  */
  // Check, must be branch or a external call.
  /*DynInstr *dynInstr = trace->back();
  Instruction *instr = idMgr.getOrigInstr(dynInstr);
  if (!Util::isBr(instr)) {
    if (!(Util::isCall(instr) && !funcSumm.isInternalCall(dynInstr))) {
      errs() << "pathId is inconsistent: " << pathId << "\n";
      dumpTrace(trace, "PathSlicer::getLatestBrOrExtCallIdx");
      stat.printDynInstr(dynInstr, "PathSlicer::getLatestBrOrExtCallIdx last one");
      abort();
    }
  }*/
    
  return trace->back()->getIndex();
}

void PathSlicer::collectExplored(llvm::Instruction *instr) {
  stat.collectExplored(instr);
}

void PathSlicer::collectStatesStat(void *pathId) {
  assert(pathId);
  ExecutionState *state = (ExecutionState *)pathId;
  stat.numStates++;
  if (state->isPruned)
	stat.numPrunedStates++;
}

bool PathSlicer::isInternalFunction(llvm::Function *f) {
  assert(f);
  return funcSumm.isInternalFunction(f);
}

void PathSlicer::getKLEEFinalStat(unsigned numInstrs, unsigned numCoveredInstrs,
      unsigned numUnCoveredInstrs, unsigned numPaths, unsigned numTests, bool finished) {
  stat.getKLEEFinalStat(numInstrs, numCoveredInstrs, numUnCoveredInstrs, numPaths, numTests, finished);
}

void PathSlicer::collectNotPrunedInstrs(void *pathId) {
  // Collect some stat only.
  if (!((ExecutionState *)pathId)->isPruned)
  	 stat.incNotPrunedStatesInstrs();
}

void PathSlicer::clearPath(void *pathId) {
  if (!DM_IN(pathId, allPathTraces))
    return;
  DynInstrVector *trace = allPathTraces[pathId];
  tgtMgr.clearTargets(pathId);
  allPathTraces.erase(pathId);
  delete trace;
}

void PathSlicer::printDynInstr(void *pathId, size_t index) {
  if (!DM_IN(pathId, allPathTraces)) {
    fprintf(stderr, "PathSlicer::printDynInstr curState %p no trace, return.\n", pathId);
    return;
  }
  DynInstrVector *trace = allPathTraces[pathId];
  if (!(trace->size() > index)) {
    fprintf(stderr, "PathSlicer::printDynInstr curState %p trace too small, return.\n", pathId);
    return;
  }
  stat.printDynInstr(trace->at(index), "PathSlicer::printDynInstr");
}

void PathSlicer::checkInstrMapConsistency(const char *tag) {
  idMgr.checkInstrMapConsistency(tag);
}

void PathSlicer::readModulePath(std::string path) {
  repair::Bc2BddPass::setModulePath(path);
}