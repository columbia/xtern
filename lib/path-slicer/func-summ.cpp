#include "func-summ.h"
using namespace tern;

using namespace llvm;

FuncSumm::FuncSumm() {
  
}

FuncSumm::~FuncSumm() {

}

bool FuncSumm::isInternalFunction(const Function *f) {
  return false; // TBD
}

bool FuncSumm::isInternalCall(const Instruction *instr) {
  return false; // TBD
}

bool FuncSumm::isInternalCall(DynInstr *dynInstr) {
  return false; // TBD
}

