#include "dyn-oprd.h"
using namespace tern;

DynOprd::DynOprd() {

}

DynOprd::DynOprd(DynInstr *dynInstr, int oprdIndex) {
  this->dynInstr = dynInstr;
  this->oprdIndex = oprdIndex;
}

DynOprd::~DynOprd() {

}

DynInstr *DynOprd::getDynInstr() {
  return dynInstr;
}

const llvm::Value *DynOprd::getStaticValue() {
  // TBD.
  return NULL;
}

bool DynOprd::isConstant() {
  const Value *v = getStaticValue();
  return isa<Constant>(v);
}

