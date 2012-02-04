#include "util.h"
#include "instr-id-mgr.h"
using namespace tern;

using namespace llvm;

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
  Util::addTargetDataToPM(origModule, origIdPm);
  origIdPm->add(origIda);
  origIdPm->run(*origModule);

   if (mxModule) {
    mxIda = new IDAssigner();
    PassManager *mxIdPm = new PassManager;
    Util::addTargetDataToPM(mxModule, mxIdPm);
    mxIdPm->add(mxIda);
    mxIdPm->run(*mxModule);
  } else
    mxIda = NULL;

   if (simModule) {
    simIda = new IDAssigner();
    PassManager *simIdPm = new PassManager;
    Util::addTargetDataToPM(simModule, simIdPm);
    simIdPm->add(simIda);
    simIdPm->run(*simModule);
  } else
    simIda = NULL;

}

Instruction *InstrIdMgr::getMxInstr(DynInstr *dynInstr) {
  return NULL; // TBD.
}

Instruction *InstrIdMgr::getOrigInstrCtx(int instrId) {
  return origIda->getInstruction(instrId);
}

Instruction *InstrIdMgr::getMxInstrCtx(int instrId) {
  return mxIda->getInstruction(instrId);
}

Instruction *InstrIdMgr::getSimInstrCtx(int instrId) {
  return simIda->getInstruction(instrId);
}

Instruction *InstrIdMgr::getOrigInstr(DynInstr *dynInstr) {
  return getOrigInstrCtx(dynInstr->getOrigInstrId());
}

int InstrIdMgr::getMxInstrId(DynInstr *dynInstr) {
  return -1;//TBD
}


