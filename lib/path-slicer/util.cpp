#include "util.h"
using namespace tern;

bool Util::isPHI(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isPHI(instr);
}

bool Util::isBr(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isBr(instr);
}

bool Util::isRet(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isRet(instr);
}

bool Util::isCall(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isCall(instr);
}

bool Util::isLoad(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isLoad(instr);
}

bool Util::isStore(DynInstr *dynInstr) {
  Instruction *instr = dynInstr->getOrigInstr();
  return isStore(instr);
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

bool Util::isPHI(const llvm::Instruction *instr) {
  return instr->getOpcode() == Instruction::PHI;
}

bool Util::isBr(const llvm::Instruction *instr) {
  return instr->getOpcode() == Instruction::Br;
}

bool Util::isRet(const llvm::Instruction *instr) {
  return instr->getOpcode() == Instruction::Ret;
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

bool Util::isMem(const llvm::Instruction *instr) {
  return isLoad(instr) || isStore(instr);
}


