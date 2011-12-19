#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "path-slicer.h"
using namespace tern;

static RegisterPass<PathSlicer> X(
		"path-slicer",
		"Tern Path Slicer",
		false,
		true); // is analysis

char PathSlicer::ID = 0;

PathSlicer::PathSlicer(): ModulePass(&ID) {
}

PathSlicer::~PathSlicer() {}

void PathSlicer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  ModulePass::getAnalysisUsage(AU);
}

bool PathSlicer::runOnModule(Module &M) {
  runPathSlicer(M);
  return false;
}

void PathSlicer::loadTrace() {
  
}

void PathSlicer::enforceRacyEdges() {
  // Enforce all racy edges, and split new regions.
}

void PathSlicer::runPathSlicer(Module &M) {
  // Load trace.
  loadTrace();
  assert(trace.size() > 0);
  
  // Enforce racy edges.
  enforceRacyEdges();
  
  // Run intra-slicer.
  interSlicer.detectInputDepRaces(&instrRegions);

  // Run inter-slicer.
  DynInstrItr itr = trace.end();
  itr--;
  intraSlicer.detectInputDepRaces(itr);

  // Calculate stat results.
  calStat();
  
}

