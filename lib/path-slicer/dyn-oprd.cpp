#include "dyn-oprd.h"
using namespace tern;

DynOprd::DynOprd() {

}

DynOprd::DynOprd(DynInstr *dynInstr, Value *staticValue, int oprdIndex) {
  this->dynInstr = dynInstr;
  this->staticValue = staticValue;
  this->oprdIndex = oprdIndex;
}

DynOprd::~DynOprd() {

}

DynInstr *DynOprd::getDynInstr() {
  return dynInstr;
}

llvm::Value *DynOprd::getStaticValue() {
  return staticValue;
}

int DynOprd::getIndex() {
  return oprdIndex;
}

bool DynOprd::isConstant() {
  return isa<Constant>(getStaticValue());
}

