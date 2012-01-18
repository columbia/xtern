#include "dyn-instrs.h"
#include "macros.h"
using namespace tern;

DynInstr::DynInstr() {

}

DynInstr::~DynInstr() {

}

Instruction *DynInstr::getOrigInstr() {
  return region->getOrigInstr(this);
}

bool DynInstr::isTarget() {
  return isTaken();
}

DynPHIInstr::DynPHIInstr() {

}

DynPHIInstr::~DynPHIInstr() {

}

void DynPHIInstr::setIncomingIndex(unsigned index) {
  incomingIndex = index;
}

unsigned DynPHIInstr::getIncomingIndex() {
  return incomingIndex;
}

DynBrInstr::DynBrInstr() {

}

DynBrInstr::~DynBrInstr() {

}

DynRetInstr::DynRetInstr() {

}

DynRetInstr::~DynRetInstr() {

}

void DynRetInstr::setDynCallInstr(DynInstr *dynInstr) {
  dynCallInstr = dynInstr;
}

DynInstr *DynRetInstr::getDynCallInstr() {
  return dynCallInstr;
}

DynCallInstr::DynCallInstr() {

}

DynCallInstr::~DynCallInstr() {

}

void DynCallInstr::setCalledFunc(llvm::Function *f) {
  calledFunc = f;
}

llvm::Function *DynCallInstr::getCalledFunc() {
  return calledFunc;
}

bool DynCallInstr::isInternalCall() {
  return (!calledFunc->isDeclaration());
}

DynSpawnThreadInstr::DynSpawnThreadInstr() {

}

DynSpawnThreadInstr::~DynSpawnThreadInstr() {

}

void DynSpawnThreadInstr::setChildTid(int childTid) {
  this->childTid = childTid;
}

int DynSpawnThreadInstr::getChildTid() {
  return childTid;
}

DynMemInstr::DynMemInstr() {

}

DynMemInstr::~DynMemInstr() {

}

void DynMemInstr::setMemAddr(long memAddr) {
  this->memAddr = memAddr;
}

long DynMemInstr::getMemAddr() {
  return memAddr;
}

void DynMemInstr::setMemAddrSymbolic(bool sym) {
  isAddrSymbolic = sym;
}

bool DynMemInstr::isMemAddrSymbolic() {
  return isAddrSymbolic;
}

