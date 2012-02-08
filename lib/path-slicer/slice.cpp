#include "slice.h"
using namespace tern;

Slice::Slice() {

}

Slice::~Slice() {

}

void Slice::init(Stat *stat) {
  this->stat = stat;
}

void Slice::add(DynInstr *dynInstr, uchar reason) {
  dynInstr->setTaken(reason);
  instsList.insert(instsList.begin(), dynInstr);
  instsSet.insert(dynInstr);
}

void Slice::dump() {
  DynInstrItr itr = begin();
  for (; itr != end(); itr++)
    stat->printDynInstr(*itr, __func__);
}

size_t Slice::size() {
  return instsList.size();
}

void Slice::clear() {
  instsList.clear();
  instsSet.clear();
}

bool Slice::in(DynInstr *dynInstr) {
  return DS_IN(dynInstr, instsSet);
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


