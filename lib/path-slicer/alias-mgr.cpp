
#include "alias-mgr.h"
#include "util.h"
using namespace tern;

using namespace repair;

using namespace llvm;

AliasMgr::AliasMgr() {
  origBAA = NULL;
  mxBAA = NULL;
  simBAA = NULL;
  numPointeeQry = 0;
  numHitPointeeQry = 0;
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

void AliasMgr::initBAA(repair::BddAliasAnalysis *BAA) {
  if (NORMAL_SLICING) {
    origBAA = BAA;
  } else if (MAX_SLICING) {
    mxBAA = BAA;
  } else {
    assert(false);  // range slicing: tbd.
  }
}

BddAliasAnalysis *AliasMgr::getBAA() {
  if (NORMAL_SLICING) {
    return origBAA;
  } else if (MAX_SLICING) {
    return mxBAA;
  } else {
    assert(false);  // range slicing: tbd.
  }
}

bool AliasMgr::mayAlias(DynOprd *dynOprd1, DynOprd *dynOprd2) {
  bool result = false;
  BEGINTIME(stat->mayAliasSt);
  DynInstr *dynInstr1 = dynOprd1->getDynInstr();
  DynInstr *dynInstr2 = dynOprd2->getDynInstr();
  //assert(Util::isMem(dynInstr1) && Util::isMem(dynInstr2));

  CallCtx *ctx1 = NULL;
  CallCtx *ctx2 = NULL;
  Instruction *instr1 = NULL;
  Instruction *instr2 = NULL;
  Value *v1 = NULL;
  Value *v2 = NULL;
  unsigned opIndex1 = dynOprd1->getIndex();
  unsigned opIndex2 = dynOprd2->getIndex();

  if (NORMAL_SLICING) {
    ctx1 = dynInstr1->getCallingCtx();
    ctx2 = dynInstr2->getCallingCtx();
    instr1 = idMgr->getOrigInstr(dynInstr1);
    instr2 = idMgr->getOrigInstr(dynInstr2);
  } else if (MAX_SLICING) {
    // In max slicing mode, getCallingCtx() is the call stack of max sliced bc.
    ctx1 = dynInstr1->getCallingCtx();  
    ctx2 = dynInstr2->getCallingCtx();
    instr1 = idMgr->getMxInstr(dynInstr1);
    instr2 = idMgr->getMxInstr(dynInstr2);
  } else if (RANGE_SLICING) {
    /* TBD: NOT SURE ABOUT THE NEW CALL CONTEXT OF ADV ALIAS.
        There could be multiple simplified calling context with respected to 
        each orig calling context, and multiple simplified instructions with 
        respected to each original instruction. */
    assert(false);
  }
  v1 = instr1->getOperand(opIndex1);
  v2 = instr2->getOperand(opIndex2);

  // Query cache first.
  if (aliasCache.in((void *)ctx1, (void *)v1, (void *)ctx2, (void *)v2, result))
    goto finish;

  // Query bdd.
  if (CTX_SENSITIVE) {
    vector<User *> userCtx1;
    vector<User *> userCtx2;
    buildUserCtx(ctx1, userCtx1);
    buildUserCtx(ctx2, userCtx2);
    result = getBAA()->alias(&userCtx1, v1, 0, &userCtx2, v2, 0);
    if (DBG) {
      fprintf(stderr, "alias query <%p (" SZ ") %p %d>, <%p (" SZ ") %p %d>: result %s\n",
        (void *)ctx1, ctx1->size(), (void *)instr1, opIndex1,
        (void *)ctx2, ctx2->size(), (void *)instr2, opIndex2,
        result?"MayAlias":"NoAlias");
      for (size_t i = 0; i < ctx1->size(); i++)
        fprintf(stderr, "CTX [" SZ "]: %d\n", i, ctx1->at(i));
      errs() << "INSTR1: " << stat->printInstr(idMgr->getOrigInstr(dynInstr1)) << "\n";
      errs() << "INSTR2: " << stat->printInstr(idMgr->getOrigInstr(dynInstr2)) << "\n";
    }
  } else
    result = getBAA()->alias(v1, 0, v2, 0);

  // Update cache.
  aliasCache.add((void *)ctx1, (void *)v1, (void *)ctx2, (void *)v2, result);

finish:
  ENDTIME(stat->mayAliasTime, stat->mayAliasSt, stat->mayAliasEnd);
  return result;
}

bool AliasMgr::mayAlias(llvm::Value *v1, llvm::Value *v2) {
  bool result = (getBAA()->getPointeeSet(NULL, v1, 0) & getBAA()->getPointeeSet(NULL, v2, 0)) != bddfalse;
  return result;
}

bdd AliasMgr::getPointTee(DynOprd *dynOprd) {
  BEGINTIME(stat->pointeeSt);
  DynInstr *dynInstr = dynOprd->getDynInstr();
  Instruction *instr = NULL;
  unsigned opIndex = dynOprd->getIndex();
  bdd retBdd = bddfalse;
  numPointeeQry++;
  Value *v = NULL;
  //if (numPointeeQry%100000 == 0)
    //fprintf(stderr, "AliasMgr::getPointTee1 %ld/%ld\n", numHitPointeeQry, numPointeeQry);

  if (NORMAL_SLICING)
    instr = idMgr->getOrigInstr(dynInstr);
  else if (MAX_SLICING)
    instr = idMgr->getMxInstr(dynInstr);      
  else
    assert(false);
  v = instr->getOperand(opIndex);

  if (CTX_SENSITIVE) {
    vector<User *> userCtx;
    CallCtx *intCtx = dynInstr->getCallingCtx();
    assert(intCtx);

    // Fast path, query bdd cache.
    if (pointeeCache.in((void *)intCtx, (void *)v)) {
      numHitPointeeQry++;
      retBdd = pointeeCache.get((void *)intCtx, (void *)v);
      goto finish;
    }

    // Slow path.
    buildUserCtx(intCtx, userCtx);
    retBdd = getBAA()->getPointeeSet(&userCtx, v, 0);

    // Update bdd cache.
    pointeeCache.add((void *)intCtx, (void *)v, retBdd);
  } else {
    // Fast path, query bdd cache.
    if (pointeeCache.in(NULL, (void *)v)) {
      numHitPointeeQry++;
      retBdd = pointeeCache.get(NULL, (void *)v);
      goto finish;
    }

    // Slow path.
    retBdd = getBAA()->getPointeeSet(NULL, v, 0);

    // Update bdd cache.
    pointeeCache.add(NULL, (void *)v, retBdd);
  }

finish:
  ENDTIME(stat->pointeeTime, stat->pointeeSt, stat->pointeeEnd);
  return retBdd;
}

bdd AliasMgr::getPointTee(DynInstr *ctxOfDynInstr, llvm::Value *v) {
  BEGINTIME(stat->pointeeSt);
  bdd retBdd = bddfalse;
  numPointeeQry++;
  //if (numPointeeQry%100 == 0)
    //fprintf(stderr, "AliasMgr::getPointTee2 %ld/%ld\n", numHitPointeeQry, numPointeeQry);
  
  if (CTX_SENSITIVE) {
    vector<User *> userCtx;
    CallCtx *intCtx = ctxOfDynInstr->getCallingCtx();
    assert(intCtx);

    // Fast path, query bdd cache.
    if (pointeeCache.in((void *)intCtx, (void *)v)) {
      numHitPointeeQry++;
      retBdd = pointeeCache.get((void *)intCtx, (void *)v);
      goto finish;
    }

	buildUserCtx(intCtx, userCtx);
    retBdd = getBAA()->getPointeeSet(&userCtx, v, 0);

    // Update bdd cache.
    pointeeCache.add((void *)intCtx, (void *)v, retBdd);
  } else {
    // Fast path, query bdd cache.
    if (pointeeCache.in(NULL, (void *)v)) {
      numHitPointeeQry++;
      retBdd = pointeeCache.get(NULL, (void *)v);
      goto finish;
    }

    retBdd = getBAA()->getPointeeSet(NULL, v, 0);

    // Update bdd cache.
    pointeeCache.add(NULL, (void *)v, retBdd);
  }

finish:
  ENDTIME(stat->pointeeTime, stat->pointeeSt, stat->pointeeEnd);
  return retBdd;
}

void AliasMgr::buildUserCtx(const std::vector<int> *intCtx, std::vector<llvm::User *> &userCtx) {
  userCtx.clear();
  if (NORMAL_SLICING) {
    for (size_t i = 0; i < intCtx->size(); i++)
      userCtx.push_back(cast<User>(idMgr->getOrigInstrCtx(intCtx->at(i))));
  } else if (MAX_SLICING) {
    for (size_t i = 0; i < intCtx->size(); i++)
      userCtx.push_back(cast<User>(idMgr->getMxInstrCtx(intCtx->at(i))));
  } else {
    assert(false);
  }
}

