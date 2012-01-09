#include "llvm/Instructions.h"
#include "llvm/Attributes.h"
#include "common/util.h"
using namespace llvm;

#include "intra-slicer.h"
using namespace tern;

IntraSlicer::IntraSlicer() {
}

IntraSlicer::~IntraSlicer() {}

void IntraSlicer::detectInputDepRaces(const DynInstrVector trace, size_t startIndex) {
  
}

