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

void InstrIdMgr::init(Stat *stat, FuncSumm *funcSumm) {
  this->stat = stat;
  this->funcSumm = funcSumm;
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

  // DBG. Print instruction pointer to id mapping.
  if (DBG)
	printInstrIdMap();

  if (mxModule) {
    mxIda = new IDAssigner();
    PassManager *mxIdPm = new PassManager;
    Util::addTargetDataToPM(mxModule, mxIdPm);
    mxIdPm->add(mxIda);
    mxIdPm->run(*mxModule);
  }

  if (simModule) {
    simIda = new IDAssigner();
    PassManager *simIdPm = new PassManager;
    Util::addTargetDataToPM(simModule, simIdPm);
    simIdPm->add(simIda);
    simIdPm->run(*simModule);
  }

}

Instruction *InstrIdMgr::getMxInstr(DynInstr *dynInstr) {
  return NULL; // TBD.
}

int InstrIdMgr::getOrigInstrId(llvm::Instruction *instr) {
  return (int)origIda->getInstructionID(instr);
}

Instruction *InstrIdMgr::getOrigInstrCtx(int instrId) {
  //fprintf(stderr, "InstrIdMgr::getOrigInstrCtx instrId %d, %p\n",
    //instrId, (void *)(origIda->getInstruction(instrId)));
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

void InstrIdMgr::checkInstrMapConsistency() {
  for (Module::iterator f = origModule->begin(), fe = origModule->end(); f != fe; ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        Instruction *instr = i;
        // Each internal instruction must belong to an internal function.
        if (getOrigInstrId(instr) != -1) {
          if (!funcSumm->isInternalFunction(f)) {
            printInstrIdMap();
            fprintf(stderr, "InstrIdMgr::checkInstrMapConsistency instruction %p must be INTERNAL. \
              LLVM linker may have randomly changed the pointer of some internal sincturctions. \
              Please make sure your /proc/sys/kernel/randomize_va_space is 0.\n", (void *)instr);
            exit(1);
          }
        }

        // Each external function must not contain any internal instruction.
        if (!funcSumm->isInternalFunction(f)) {
          if (getOrigInstrId(instr) != -1) {
            printInstrIdMap();
            fprintf(stderr, "InstrIdMgr::checkInstrMapConsistency instruction %p must be EXTERNAL. \
              LLVM linker may have randomly changed the pointer of some internal sincturctions. \
              Please make sure your /proc/sys/kernel/randomize_va_space is 0.\n", (void *)instr);
            exit(1);
          }
        }
      }
}

void InstrIdMgr::printInstrIdMap() {
for (Module::iterator f = origModule->begin(), fe = origModule->end(); f != fe; ++f)
	    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
	      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
	        Instruction *instr = i;
	        errs() << "InstrIdMgr::printInstrIdMap: F:" << f->getNameStr() << ":BB:" << b->getNameStr() << ":" 
            << (void *)instr << " --> " <<  getOrigInstrId(instr) << "\n";
	      }
}

