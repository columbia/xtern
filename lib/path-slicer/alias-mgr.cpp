
#include "alias-mgr.h"
#include "util.h"
using namespace tern;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

using namespace llvm;

AliasMgr::AliasMgr() {
  origAaol = NULL;
  mxAaol = NULL;
  simAaol = NULL;
  //advAlias = NULL;
}

AliasMgr::~AliasMgr() {

}

void AliasMgr::initStat(Stat *stat) {
  this->stat = stat;
}

void AliasMgr::initInstrIdMgr(InstrIdMgr *idMgr) {
  this->idMgr = idMgr;
}

void AliasMgr::initModules(Module *origModule, Module *mxModule,
    Module *simModule) {
  fprintf(stderr, "AliasMgr::initModules begin %p, %p, %p\n", (void *)origModule, 
    (void *)mxModule, (void *)simModule);
  // Init the three AAOLs.
  assert(origModule);
  origAaol = initAAOL(origModule);
  if (mxModule)
    mxAaol = initAAOL(mxModule);
  if (simModule)
    simAaol = initAAOL(simModule);
  
  // Init adv alias.
  // TBD.
}

AAOLClient *AliasMgr::initAAOL(llvm::Module *module) {
  fprintf(stderr, "AliasMgr::initAAOL begin\n");
  std::string llvmRoot(getenv("LLVM_ROOT"));
  assert(llvmRoot != "");
  std::string aaolLib = llvmRoot + "/install/lib/libaaol.so";
  std::string idmLib = llvmRoot + "/install/lib/libid-manager.so";
  std::string libPath = llvmRoot + "/install/lib/libbc2bdd.so";

  std::map<Instruction *, int> instrMap;
  genInstrMap(*module, instrMap);
  AAOLClient *aaol = new AAOLClient(aaolLib.c_str(), idmLib.c_str(), libPath.c_str(),
    "-bc2bdd-aa", *module, instrMap);
  if(!aaol->begin())
    exit(1);
  aaol->setDebugLevel(get_option(tern_path_slicer, aaol_dbg_level));
  fprintf(stderr, "AliasMgr::initAAOL end\n");
  return aaol;
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
  //assert(Util::isMem(dynInstr1) && Util::isMem(dynInstr2));

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
    instrId1 = idMgr->getMxInstrId(dynInstr1);
    instrId2 = idMgr->getMxInstrId(dynInstr2);
  } else if (RANGE_SLICING) {
    /* TBD: NOT SURE ABOUT THE NEW CALL CONTEXT OF ADV ALIAS.
        There could be multiple simplified calling context with respected to 
        each orig calling context, and multiple simplified instructions with 
        respected to each original instruction. */
    assert(false);
  }

  // Query cache first.
  if (aliasCache.in((void *)ctx1, (void *)instrId1, (void *)ctx2, (void *)instrId2, result))
    return result;

  // Query bdd.
  if (CTX_SENSITIVE) {
    if (DBG) {
      fprintf(stderr, "alias query <%p (" SZ ") %d %d>, <%p (" SZ ") %d %d>\n",
        (void *)ctx1, ctx1->size(), instrId1, opIndex1,
        (void *)ctx2, ctx2->size(), instrId2, opIndex2);
      for (size_t i = 0; i < ctx1->size(); i++)
        fprintf(stderr, "CTX [" SZ "]: %d\n", i, ctx1->at(i));
      errs() << "INSTR1: " << stat->printInstr(idMgr->getOrigInstr(dynInstr1)) << "\n";
      errs() << "INSTR2: " << stat->printInstr(idMgr->getOrigInstr(dynInstr2)) << "\n";
    }
    result = origAaol->aliasQuery(*ctx1, instrId1, opIndex1, *ctx2, instrId2, opIndex2);
  } else
    result = origAaol->aliasQuery(instrId1, opIndex1, instrId2, opIndex2);

  // Update cache.
  aliasCache.add((void *)ctx1, (void *)instrId1, (void *)ctx2, (void *)instrId2, result);
  
  return result;
}

bool AliasMgr::mayAlias(llvm::Value *v1, llvm::Value *v2) {
  BddAliasAnalysis *baa = NULL;
  if (NORMAL_SLICING) {
    baa = (BddAliasAnalysis *)(origAaol->AAPass);
  } else if (MAX_SLICING) {
    baa = (BddAliasAnalysis *)(mxAaol->AAPass);
  } else {
    baa = (BddAliasAnalysis *)(simAaol->AAPass);
    assert(false);  // range slicing: tbd.
  }
  return (baa->getPointeeSet(NULL, v1, 0) & baa->getPointeeSet(NULL, v2, 0)) != bddfalse;
}

bdd AliasMgr::getPointTee(DynOprd *dynOprd) {
  DynInstr *dynInstr = dynOprd->getDynInstr();
  Instruction *instr = NULL;
  unsigned opIndex = dynOprd->getIndex();
  BddAliasAnalysis *baa = NULL;
  
  if (CTX_SENSITIVE) {
    vector<User *> usrCtx;
    CallCtx *intCtx = dynInstr->getCallingCtx();
    assert(intCtx);
    if (NORMAL_SLICING) {
      baa = (BddAliasAnalysis *)(origAaol->AAPass);
      for (size_t i = 0; i < intCtx->size(); i++)
        usrCtx.push_back(cast<User>(idMgr->getOrigInstrCtx(intCtx->at(i))));
      instr = idMgr->getOrigInstr(dynInstr);
    } else if (MAX_SLICING) {
      baa = (BddAliasAnalysis *)(mxAaol->AAPass);
      for (size_t i = 0; i < intCtx->size(); i++)
        usrCtx.push_back(cast<User>(idMgr->getMxInstrCtx(intCtx->at(i))));
      instr = idMgr->getMxInstr(dynInstr);      
    } else {
      baa = (BddAliasAnalysis *)(simAaol->AAPass);
      assert(false);  // range slicing: tbd.
    }

    return baa->getPointeeSet(&usrCtx, instr->getOperand(opIndex), 0);
  } else {
    if (NORMAL_SLICING) {
      baa = (BddAliasAnalysis *)(origAaol->AAPass);
      instr = idMgr->getOrigInstr(dynInstr);
    } else if (MAX_SLICING) {
      baa = (BddAliasAnalysis *)(mxAaol->AAPass);
      instr = idMgr->getMxInstr(dynInstr);      
    } else {
      baa = (BddAliasAnalysis *)(simAaol->AAPass);
      assert(false);  // range slicing: tbd.
    }

    return baa->getPointeeSet(NULL, instr->getOperand(opIndex), 0);
  }
}

bdd AliasMgr::getPointTee(DynInstr *ctxOfDynInstr, llvm::Value *v) {
  BddAliasAnalysis *baa = NULL;
  
  if (CTX_SENSITIVE) {
    vector<User *> usrCtx;
    CallCtx *intCtx = ctxOfDynInstr->getCallingCtx();
    assert(intCtx);
    if (NORMAL_SLICING) {
      baa = (BddAliasAnalysis *)(origAaol->AAPass);
      for (size_t i = 0; i < intCtx->size(); i++)
        usrCtx.push_back(cast<User>(idMgr->getOrigInstrCtx(intCtx->at(i))));
    } else if (MAX_SLICING) {
      baa = (BddAliasAnalysis *)(mxAaol->AAPass);
      for (size_t i = 0; i < intCtx->size(); i++)
        usrCtx.push_back(cast<User>(idMgr->getMxInstrCtx(intCtx->at(i))));
    } else {
      baa = (BddAliasAnalysis *)(simAaol->AAPass);
      assert(false);  // range slicing: tbd.
    }

    return baa->getPointeeSet(&usrCtx, v, 0);
  } else {
    if (NORMAL_SLICING) {
      baa = (BddAliasAnalysis *)(origAaol->AAPass);
    } else if (MAX_SLICING) {
      baa = (BddAliasAnalysis *)(mxAaol->AAPass);
    } else {
      baa = (BddAliasAnalysis *)(simAaol->AAPass);
      assert(false);  // range slicing: tbd.
    }

    return baa->getPointeeSet(NULL, v, 0);
  }
}


