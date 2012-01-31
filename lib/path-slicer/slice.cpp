#include "slice.h"
using namespace tern;

Slice::Slice() {

}

Slice::~Slice() {

}

void Slice::add(DynInstr *dynInstr, unsigned char reason) {

}

DynInstr *Slice::getHead() {
  DynInstrItr itr = begin();
  return *itr;
}

DynInstrItr Slice::begin() {
  return instsList.begin();
}

DynInstrItr Slice::end() {
  return instsList.end();
}


