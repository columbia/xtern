#include "instr-id-mgr.h"
using namespace tern;

InstrIdMgr::InstrIdMgr() {

}

InstrIdMgr::~InstrIdMgr() {

}

void InstrIdMgr::initStat(Stat *stat) {
  this->stat = stat;
}

void InstrIdMgr::initModules(llvm::Module *origModule, llvm::Module *mxModule,
  llvm::Module *simModule, const char *lmTracePath) {

}

