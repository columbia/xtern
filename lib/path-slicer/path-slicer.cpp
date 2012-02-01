#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CallSite.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
using namespace llvm;

#include "path-slicer.h"
using namespace tern;

using namespace klee;

static RegisterPass<PathSlicer> X(
		"path-slicer",
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

}

PathSlicer::~PathSlicer() {

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

  /* Init instruction id manager. */
  idMgr.initStat(&stat);
  idMgr.initModules(origModule, mxModule, simModule, LmTrace.c_str());

  /* Init call stack manager. */
  ctxMgr.initStat(&stat);
  ctxMgr.initInstrIdMgr(&idMgr);

  /* Init alias manager. */
  aliasMgr.initStat(&stat);
  aliasMgr.initInstrIdMgr(&idMgr);
  aliasMgr.initModules(origModule, mxModule, simModule);

  /* Init CFG manager. */
  cfgMgr.initStat(&stat);
  PassManager *cfgPM = new PassManager;
  cfgPM->add(&cfgMgr);
  cfgPM->run(*origModule);

  /* Init oprd summary. */
  oprdSumm.initStat(&stat);
  PassManager *oprdPM = new PassManager;
  oprdPM->add(&oprdSumm);
  if (NORMAL_SLICING) {
    collectInternalFunctions(*origModule);
    oprdPM->run(*origModule);
    
  } else if (MAX_SLICING) {
    collectInternalFunctions(*mxModule);
    oprdPM->run(*mxModule);
  } else {
    collectInternalFunctions(*simModule);
    oprdPM->run(*simModule);
    assert(false);// TBD. NOT SURE WHETHER SHOULD PASS IN MX OR SIM MODULE HERE.
  }

  /* Init trace util. */
  if (KLEE_RECORDING)
    traceUtil = new KleeTraceUtil();
  else if (XTERN_RECORDING)
    traceUtil = new XTernTraceUtil();
  else
    assert(false);
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
  // Enforce all racy edges, and split new regions.
}

void PathSlicer::runPathSlicer(void *pathId, set<BranchInst *> &brInstrs) {
  // Get trace of current path and do some pre-processing.
  assert(DM_IN(pathId, allPathTraces));
  DynInstrVector *trace = allPathTraces[pathId];
  assert(trace->size() > 0);
  traceUtil->preProcess(trace);
  
  // Enforce racy edges.
  enforceRacyEdges();
  
  // Run inter-slicer.
  interSlicer.detectInputDepRaces(&instrRegions);

  // Run intra-slicer.
  size_t startIndex = trace->size();
  assert(startIndex > 0);
  startIndex--;
  intraSlicer.init(this, trace, startIndex); // Init intra threas slicer.
  // TBD. Take initial instruction and add init oprds.
  intraSlicer.detectInputDepRaces(); // Detect input dependent races.

  // Calculate stat results.
  calStat();

  // Free the trace along current path.
  freeCurPathTrace(pathId);
}

void PathSlicer::freeCurPathTrace(void *pathId) {
  assert(DM_IN(pathId, allPathTraces));
  DynInstrVector *trace = allPathTraces[pathId];
  for (size_t i = 0; i < trace->size(); i++)
    delete trace->at(i);
  trace->clear();
  allPathTraces.erase(pathId);
}

void PathSlicer::collectInternalFunctions(Module &M) {
  for (Module::iterator f = M.begin(); f != M.end(); ++f) {
    if (!f->isDeclaration())
      internalFunctions.insert(f);
  }
}

bool PathSlicer::isInternalCall(const Instruction *instr) {
  const CallInst *ci = dyn_cast<CallInst>(instr);
  assert(ci);
  const Function *f = ci->getCalledFunction();  
  if (!f) // If it is an indirect call (function pointer), return false conservatively.
    return false;
  else
    return isInternalFunction(f);
}

bool PathSlicer::isInternalCall(DynInstr *dynInstr) {
  DynCallInstr *callInstr = (DynCallInstr*)dynInstr;
  Function *f = callInstr->getCalledFunc();
  return isInternalFunction(f);
}

bool PathSlicer::isInternalFunction(const Function *f) {
  // TBD: If the called function is a function pointer, would "f" be NULL?
  return !f->isDeclaration() && internalFunctions.count(f) > 0;
}

void PathSlicer::calStat() {
  
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

