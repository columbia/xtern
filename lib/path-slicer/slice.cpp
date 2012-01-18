#include "slice.h"
using namespace tern;

Slice::Slice() {

}

Slice::~Slice() {

}

void Slice::add(DynInstr *dynInstr, const char *reason) {

}

DynInstr *Slice::getHead() {
  DynInstrItr itr = begin();
  return *itr;
}


