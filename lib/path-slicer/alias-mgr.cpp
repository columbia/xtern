
#include "alias-mgr.h"
using namespace tern;

AliasMgr::AliasMgr() {
  origAaol = NULL;
  mxAaol = NULL;
  simAaol = NULL;
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
  return false;
}

