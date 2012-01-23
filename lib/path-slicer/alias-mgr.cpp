
#include "alias-mgr.h"
#include "util.h"
using namespace tern;

AliasMgr::AliasMgr() {
  origAaol = NULL;
  mxAaol = NULL;
  simAaol = NULL;
  advAlias = NULL;
}

AliasMgr::~AliasMgr() {

}

void AliasMgr::initStat(Stat *stat) {
  this->stat = stat;
}

void AliasMgr::initInstrIdMgr(InstrIdMgr *idMgr) {
  this->idMgr = idMgr;
}

void AliasMgr::initModules(llvm::Module *origModule, llvm::Module *mxModule,
      llvm::Module *simModule) {
  // Init the three AAOLs.
  assert(origModule);
  initAAOL(&origAaol, origModule);
  if (mxModule)
    initAAOL(&mxAaol, mxModule);
  if (simModule)
    initAAOL(&simAaol, simModule);

  // Init adv alias.
  // TBD.
}

void AliasMgr::initAAOL(AAOLClient **aaol, llvm::Module *module) {
  std::string llvmRoot(getenv("LLVM_ROOT"));
  assert(llvmRoot != "");
  std::string aaolLib = llvmRoot + "/install/lib/libaaol.so";
  std::string idmLib = llvmRoot + "/install/lib/libid-manager.so";
  std::string libPath = llvmRoot + "/install/lib/libbc2bdd.so";

  std::map<Instruction *, int> instrMap;
  genInstrMap(*module, instrMap);
  (*aaol) = new AAOLClient(aaolLib.c_str(), idmLib.c_str(), libPath.c_str(),
    "-bc2bdd-aa", *module, instrMap);
  if(!(*aaol)->begin())
    exit(1);
  (*aaol)->setDebugLevel(get_option(tern_path_slicer, aaol_dbg_level));
}

void AliasMgr::genInstrMap(Module &module,
  std::map<Instruction *, int> &insMap) {
  PassManager pm;
  IDAssigner *idm = new IDAssigner();
  pm.add(idm);
  pm.run(module);

  insMap.clear();
  for (Module::iterator f = module.begin(), fe = module.end(); f != fe; ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i)
        insMap[i] = idm->getInstructionID(i);
}

bool AliasMgr::mayAlias(DynOprd *dynOprd1, DynOprd *dynOprd2) {
  bool result = false;
  DynInstr *dynInstr1 = dynOprd1->getDynInstr();
  DynInstr *dynInstr2 = dynOprd2->getDynInstr();
  assert(Util::isMem(dynInstr1) && Util::isMem(dynInstr2));

  CallCtx *ctx1 = NULL;
  CallCtx *ctx2 = NULL;
  int instrId1 = -1;
  int instrId2 = -1;
  int opIndex1 = dynOprd1->getIndex();
  int opIndex2 = dynOprd2->getIndex();

  if (NORMAL_SLICING) {
    ctx1 = dynInstr1->getCallingCtx();
    ctx2 = dynInstr2->getCallingCtx();
    instrId1 = dynInstr1->getOrigInstrId();
    instrId2 = dynInstr2->getOrigInstrId();
  } else if (MAX_SLICING) {
    // In max slicing mode, getCallingCtx() is the call stack of max sliced bc.
    ctx1 = dynInstr1->getCallingCtx();  
    ctx2 = dynInstr2->getCallingCtx();
    instrId1 = dynInstr1->getMxInstrId();
    instrId2 = dynInstr2->getMxInstrId();
  } else if (RANGE_SLICING) {
    /* TBD: NOT SURE ABOUT THE NEW CALL CONTEXT OF ADV ALIAS.
        There could be multiple simplified calling context with respected to 
        each orig calling context, and multiple simplified instructions with 
        respected to each original instruction.
    */
    assert(false);
  }

  // Query cache first.
  if (aliasCache.in((long)ctx1, instrId1, (long)ctx2, instrId2, result))
    return result;

  // Query bdd.
  if (get_option(tern_path_slicer, context_sensitive_ailas_query) == 1)
    result = origAaol->aliasQuery(*ctx1, instrId1, opIndex1, *ctx2, instrId2, opIndex2);
  else
    result = origAaol->aliasQuery(instrId1, opIndex1, instrId2, opIndex2);

  // Update cache.
  aliasCache.add((long)ctx1, instrId1, (long)ctx2, instrId2, result);
  
  return result;
}

