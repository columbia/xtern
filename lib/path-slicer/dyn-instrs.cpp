#include "dyn-instrs.h"
#include "macros.h"
using namespace tern;

DynInstr::DynInstr() {
  region = NULL;
  index = SIZE_T_INVALID;
  callingCtx = NULL;
  simCallingCtx = NULL;
}

DynInstr::~DynInstr() {

}

int DynInstr::getTid() {
  return region->getTid();
}

size_t DynInstr::getIndex() {
  return index;
}

void DynInstr::setCallingCtx(std::vector<int> *ctx) {
  this->callingCtx = ctx;
}

CallCtx *DynInstr::getCallingCtx() {
  return callingCtx;
}

void DynInstr::setSimCallingCtx(std::vector<int> *ctx) {
  this->simCallingCtx = ctx;
}

CallCtx *DynInstr::getSimCallingCtx() {
  return simCallingCtx;
}

int DynInstr::getOrigInstrId() {
  return -1; // TBD
}

int DynInstr::getMxInstrId() {
  return -1; // TBD
}

std::set<int> *DynInstr::getSimInstrId() {
  return NULL; // TBD
}

void DynInstr::setTaken(bool isTaken, const char *reason) {
  region->setTaken(this, isTaken, reason);
}

bool DynInstr::isTaken() {
  return region->isTaken(this);
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
  /* Pay attention: this function may have problem if the path slicer is called 
  with in C++ code and the passed in module has linked with uclibc. */
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

