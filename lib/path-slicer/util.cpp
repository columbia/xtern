#include "util.h"
using namespace tern;

bool Util::isPHI(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return instr->getOpcode() == Instruction::PHI;
}

bool Util::isBr(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return instr->getOpcode() == Instruction::Br;
}

bool Util::isRet(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return instr->getOpcode() == Instruction::Ret;
}

bool Util::isCall(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return instr->getOpcode() == Instruction::Call;
}

bool Util::isLoad(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return instr->getOpcode() == Instruction::Load;
}

bool Util::isStore(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return instr->getOpcode() == Instruction::Store;
}

bool Util::isMem(DynInstr *dynInstr) {
  return isLoad(dynInstr) || isStore(dynInstr);
}
