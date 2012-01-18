#include "dyn-oprd.h"
using namespace tern;

DynOprd::DynOprd() {

}

DynOprd::~DynOprd() {

}

DynInstr *DynOprd::getDynInstr() {
  return dynInstr;
}

llvm::Value *DynOprd::getStaticValue() {
  // TBD.
  return NULL;
}

bool DynOprd::isConstant() {
  Value *v = getStaticValue();
  return isa<Constant>(v);
}
