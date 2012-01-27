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

llvm::Function *Util::getFunction(llvm::Instruction *instr) {
  return instr->getParent()->getParent();
}

llvm::BasicBlock *Util::getBasicBlock(llvm::Instruction *instr) {
  return instr->getParent();
}

const llvm::Function *Util::getFunction(const llvm::Instruction *instr) {
  return instr->getParent()->getParent();
}

const llvm::BasicBlock *Util::getBasicBlock(const llvm::Instruction *instr) {
  return instr->getParent();
}

bool Util::isCall(llvm::Instruction *instr) {
  return isCall(instr);
}

bool Util::isCall(const llvm::Instruction *instr) {
  return instr->getOpcode() == Instruction::Call;
}

bool Util::isLoad(const llvm::Instruction *instr) {
  return instr->getOpcode() == Instruction::Load;
}

bool Util::isStore(const llvm::Instruction *instr) {
  return instr->getOpcode() == Instruction::Store;
}

