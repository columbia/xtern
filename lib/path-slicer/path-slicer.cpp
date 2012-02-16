#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CallSite.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
using namespace llvm;

#include "util.h"
#include "path-slicer.h"
using namespace tern;

using namespace klee;

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

const char *takenReasons[NUM_TAKEN_FLAGS];

void initTakenReasons() {
  takenReasons[NOT_TAKEN] = "NOT_TAKEN";

  takenReasons[START_TARGET] = "START_TARGET";
  takenReasons[TAKEN_EVENT] = "TAKEN_EVENT";
  takenReasons[TAKEN_RACE] = "TAKEN_RACE";

  takenReasons[INTER_INSTR2] = "INTER_INSTR2";
  takenReasons[INTER_LOAD_TGT] = "INTER_LOAD_TGT";
  takenReasons[INTER_STORE_TGT] = "INTER_STORE_TGT";

  takenReasons[INTER_BR_INSTR] = "INTER_BR_INSTR";

  takenReasons[INTER_BR_BR] = "INTER_BR_BR";

  takenReasons[INTRA_ALLOCA] = "INTRA_ALLOCA";

  takenReasons[INTRA_PHI] = "INTRA_PHI";
  takenReasons[INTRA_PHI_BR_CTRL_DEP] = "INTRA_PHI_BR_CTRL_DEP";

  takenReasons[INTRA_BR_N_POSTDOM] = "INTRA_BR_N_POSTDOM";
  takenReasons[INTRA_BR_EVENT_BETWEEN] = "INTRA_BR_EVENT_BETWEEN";
  takenReasons[INTRA_BR_WR_BETWEEN] = "INTRA_BR_WR_BETWEEN";

  takenReasons[INTRA_RET_REG_OW] = "INTRA_RET_REG_OW";
  takenReasons[INTRA_RET_CALL_EVENT] = "INTRA_RET_CALL_EVENT";
  takenReasons[INTRA_RET_WRITE_FUNC] = "INTRA_RET_WRITE_FUNC";
  takenReasons[INTRA_RET_BOTH] = "INTRA_RET_BOTH";

  takenReasons[INTRA_CALL] = "INTRA_CALL";

  takenReasons[INTRA_LOAD_OW] = "INTRA_LOAD_OW";

  takenReasons[INTRA_STORE_OW] = "INTRA_STORE_OW";
  takenReasons[INTRA_STORE_ALIAS] = "INTRA_STORE_ALIAS";

  takenReasons[INTRA_NON_MEM] = "INTRA_NON_MEM";
}

PathSlicer::PathSlicer(): ModulePass(&ID) {
  fprintf(stderr, "PathSlicer::PathSlicer()\n");
  initTakenReasons();
  load_options("local.options");
}

PathSlicer::~PathSlicer() {
  fprintf(stderr, "PathSlicer::~PathSlicer()\n");
}

void PathSlicer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

bool PathSlicer::runOnModule(Module &M) {
  init(M);
  //runPathSlicer(M);
  return false;
}

void PathSlicer::init(llvm::Module &M) {
  fprintf(stderr, "PathSlicer::init begin\n");
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
  
  /* Init function summary. */  
  PassManager *funcPM = new PassManager;
  Util::addTargetDataToPM(origModule, funcPM);
  funcPM->add(&funcSumm);
  funcPM->run(*origModule);

  /* Init oprd summary. */
  oprdSumm.init(&stat, &funcSumm, &aliasMgr, &idMgr);
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
  idMgr.initStat(&stat);
  idMgr.initModules(origModule, mxModule, simModule, LmTrace.c_str());

  /* Init call stack manager. */
  ctxMgr.init(&stat, &idMgr, &funcSumm);

  /* Init CFG manager. */
  cfgMgr.initStat(&stat);
  PassManager *cfgPM = new PassManager;
  Util::addTargetDataToPM(origModule, cfgPM);
  cfgPM->add(&cfgMgr);
  cfgPM->run(*origModule);
  fprintf(stderr, "PathSlicer::init 2\n");

  /* Init alias manager. */
  aliasMgr.initStat(&stat);
  aliasMgr.initInstrIdMgr(&idMgr);
  aliasMgr.initModules(origModule, mxModule, simModule);
  fprintf(stderr, "PathSlicer::init 1\n");

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

  // Init stat.
  stat.init(&idMgr, &ctxMgr);
  
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

void PathSlicer::runPathSlicer(void *pathId, set<BranchInst *> &brInstrs) {  
  // Get trace of current path and do some pre-processing.
  assert(DM_IN(pathId, allPathTraces));
  DynInstrVector *trace = allPathTraces[pathId];
  if (trace->size() == 0)
    return;
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

    // (2) Init slicing start index (target instruction).
    size_t startIndex = trace->size();
    assert(startIndex > 0);
    startIndex--;

    /* (3) Init slicing sub modules. This function will also clean live set and 
      slice set in the intra-thread slicer. */
    intraSlicer.init((ExecutionState *)pathId, &oprdSumm, &funcSumm,
      &aliasMgr, &idMgr, &cfgMgr, &stat, trace, startIndex);

    // (4) Take target instruction, and add dyn oprds of the instruction, depending on slicing goals.
    intraSlicer.takeStartTarget(trace->at(startIndex));

    // (5) Do intra-thread slicing.
    intraSlicer.detectInputDepRaces(tid);
  } while (0);
  
  // Calculate stat results.
  calStat();

  // Free the trace along current path. 
  traceUtil->postProcess(trace);
  allPathTraces.erase(pathId);
  delete trace;
}

void PathSlicer::calStat() {
  interSlicer.calStat();
  errs() << BAN;
  intraSlicer.calStat();
}

void PathSlicer::initKModule(KModule *kmodule) {
  if (KLEE_RECORDING)
    ((KleeTraceUtil *)traceUtil)->initKModule(kmodule);
  else
    assert(false);
}

void PathSlicer::record(void *pathId, void *instr, void *state, void *f) {
  if (!DM_IN(pathId, allPathTraces))
    allPathTraces[pathId] = new DynInstrVector;
  traceUtil->record(allPathTraces[pathId], instr, state, f);
}

void PathSlicer::copyTrace(void *newPathId, void *curPathId) {
  assert (!DM_IN(newPathId, allPathTraces));
  allPathTraces[newPathId] = new DynInstrVector;
  DynInstrVector *newTrace = allPathTraces[newPathId];
  DynInstrVector *curTrace = allPathTraces[curPathId];
  newTrace->insert(newTrace->begin(), curTrace->begin(), curTrace->end());
}

