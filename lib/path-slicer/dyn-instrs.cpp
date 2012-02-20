#include "dyn-instrs.h"
#include "macros.h"
using namespace tern;

using namespace klee;

DynInstr::DynInstr() {
  //region = NULL;
  index = SIZE_T_INVALID;
  callingCtx = NULL;
  //simCallingCtx = NULL;
  instrId = -1;
  tid = -1;
  takenFlag = TakenFlags::NOT_TAKEN;
}

DynInstr::~DynInstr() {

}

void DynInstr::setTid(uchar tid) {
  this->tid = tid;
}

uchar DynInstr::getTid() {
  return tid;
}

void DynInstr::setIndex(size_t index) {
  this->index = index;
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

/*void DynInstr::setSimCallingCtx(std::vector<int> *ctx) {
  this->simCallingCtx = ctx;
}

CallCtx *DynInstr::getSimCallingCtx() {
  return simCallingCtx;
}*/

void DynInstr::setOrigInstrId(int instrId) {
  this->instrId = instrId;
}

int DynInstr::getOrigInstrId() {
  return instrId;
}

void DynInstr::setTaken(uchar takenFlag) {
  this->takenFlag = takenFlag;
}

bool DynInstr::isTaken() {
  return takenFlag != TakenFlags::NOT_TAKEN;
}

uchar DynInstr::getTakenFlag() {
  return takenFlag;
}

const char *DynInstr::takenReason() {
  return TakenFlags::getReason(takenFlag);
}

bool DynInstr::isInterThreadTarget() {
  return isTaken() &&
    TakenFlags::getReasonKind(takenFlag) == TakenFlags::InterThreadTarget;
}

bool DynInstr::isCheckerTarget() {
  return isTaken() && 
    TakenFlags::getReasonKind(takenFlag) == TakenFlags::CheckerTarget;
}

bool DynInstr::isTarget() {
  return isTaken() && TakenFlags::isTarget(takenFlag);
}

DynPHIInstr::DynPHIInstr() {
  incomingIndex = (uchar)-1;
}

DynPHIInstr::~DynPHIInstr() {

}

void DynPHIInstr::setIncomingIndex(uchar index) {
  incomingIndex = index;
}

uchar DynPHIInstr::getIncomingIndex() {
  return incomingIndex;
}

DynBrInstr::DynBrInstr() {
  // Do not need to init condition here.
  successor = NULL;
}

DynBrInstr::~DynBrInstr() {

}

bool DynBrInstr::isSymbolicBr() {
  return !isa<klee::ConstantExpr>(condition);
}

void DynBrInstr::setBrCondition(klee::ref<klee::Expr> condition) {
  this->condition = condition;
}

klee::ref<klee::Expr> DynBrInstr::getBrCondition() {
  return condition;
}

void DynBrInstr::setSuccessorBB(BasicBlock *successor) {
  this->successor = successor;
}

BasicBlock *DynBrInstr::getSuccessorBB() {
  return successor;
}

DynRetInstr::DynRetInstr() {
  dynCallInstr = NULL;
}

DynRetInstr::~DynRetInstr() {

}

void DynRetInstr::setDynCallInstr(DynCallInstr *dynInstr) {
  dynCallInstr = dynInstr;
}

DynCallInstr *DynRetInstr::getDynCallInstr() {
  return dynCallInstr;
}

DynCallInstr::DynCallInstr() {
  calledFunc = NULL;
  containTarget = false;
}

DynCallInstr::~DynCallInstr() {

}

void DynCallInstr::setCalledFunc(llvm::Function *f) {
  calledFunc = f;
}

llvm::Function *DynCallInstr::getCalledFunc() {
  return calledFunc;
}

void DynCallInstr::setContainTarget(bool containTarget) {
  assert(containTarget);
  this->containTarget = containTarget;
}

bool DynCallInstr::getContainTarget() {
  return containTarget;
}

DynSpawnThreadInstr::DynSpawnThreadInstr() {
  childTid = -1;
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
  // DO NOT INIT SYMADDR YET.
}

DynMemInstr::~DynMemInstr() {

}

void DynMemInstr::setAddr(ref<klee::Expr> addr) {
  this->addr = addr;
}

ref<klee::Expr> DynMemInstr::getAddr() {
  return addr;
}

bool DynMemInstr::isAddrSymbolic() {
  return isa<klee::ConstantExpr>(addr);
}

