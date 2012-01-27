#include "instr-id-mgr.h"
using namespace tern;

InstrIdMgr::InstrIdMgr() {
  origModule = mxModule = simModule = NULL;
  origIda = mxIda = simIda = NULL;
  lmTracePath = "";
}

InstrIdMgr::~InstrIdMgr() {

}

void InstrIdMgr::initStat(Stat *stat) {
  this->stat = stat;
}

void InstrIdMgr::initModules(llvm::Module *origModule, llvm::Module *mxModule,
  llvm::Module *simModule, const char *lmTracePath) {
  this->origModule = origModule;
  this->mxModule = mxModule;
  this->simModule = simModule;
  this->lmTracePath = lmTracePath;
  
  assert(origModule);
  origIda = new IDAssigner();
  PassManager *origIdPm = new PassManager;
  origIdPm->add(origIda);
  origIdPm->run(*origModule);

   if (mxModule) {
    mxIda = new IDAssigner();
    PassManager *mxIdPm = new PassManager;
    mxIdPm->add(mxIda);
    mxIdPm->run(*mxModule);
  } else
    mxIda = NULL;

   if (simModule) {
    simIda = new IDAssigner();
    PassManager *simIdPm = new PassManager;
    simIdPm->add(simIda);
    simIdPm->run(*simModule);
  } else
    simIda = NULL;

}

Instruction *InstrIdMgr::getOrigInstr(int instrId) {
  return origIda->getInstruction(instrId);
}

Instruction *InstrIdMgr::getMxInstr(int instrId) {
  return mxIda->getInstruction(instrId);
}

Instruction *InstrIdMgr::getSimInstr(int instrId) {
  return simIda->getInstruction(instrId);
}

