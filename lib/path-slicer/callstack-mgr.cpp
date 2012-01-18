#include "callstack-mgr.h"
using namespace tern;

CallStackMgr::CallStackMgr() {

}

CallStackMgr::~CallStackMgr() {

}

void CallStackMgr::initStat(Stat *stat) {
  this->stat = stat;
}

void CallStackMgr::initInstrIdMgr(InstrIdMgr *idMgr) {
  this->idMgr = idMgr;
}

