#include "util.h"
#include "callstack-mgr.h"
using namespace tern;

using namespace std;

CallStackMgr::CallStackMgr() {

}

CallStackMgr::~CallStackMgr() {

}

void CallStackMgr::clear() {
  // Clear tidToCallSeqMap.
  DenseMapIterator<int, std::vector<DynCallInstr *> *> itr(tidToCallSeqMap.begin());
  for (; itr != tidToCallSeqMap.end(); itr++)
    delete itr->second;
  tidToCallSeqMap.clear();

  // Clear tidToCurCtxMap.
  tidToCurCtxMap.clear();

  // Clear ctx pool.
  DenseMapIterator<int, llvm::DenseSet<std::vector<int> * > *> itrPool(ctxPool.begin());
  for (; itrPool != ctxPool.end(); itrPool++) {
    DenseSet<vector<int> * >::iterator setItr(itrPool->second->begin());
    for (; setItr != itrPool->second->end(); ++setItr)
      delete *setItr;
    delete itrPool->second;
  }
  ctxPool.clear();

  // Clear sim ctx pool.
  // TBD.
}

void CallStackMgr::init(Stat *stat, InstrIdMgr *idMgr,
  FuncSumm *funcSumm, TargetMgr *tgtMgr) {
  this->stat = stat;
  this->idMgr = idMgr;
  this->funcSumm = funcSumm;
  this->tgtMgr = tgtMgr;
}

void CallStackMgr::updateCallStack(DynInstr *dynInstr) {
  uchar tid = dynInstr->getTid();
  std::vector<DynCallInstr *> *curSeq = tidToCallSeqMap[tid];
  Instruction *instr = idMgr->getOrigInstr(dynInstr);

  // Update call seq, push or pop.
  if (Util::isCall(instr)) {
    if (funcSumm->isInternalCall((DynCallInstr *)dynInstr)) {
      curSeq->push_back((DynCallInstr *)dynInstr);  // Push.
    } else
      return;   // if it is external call or intrinsic call, no operation.
  } else {
    assert(Util::isRet(instr));
    if (curSeq->size() > 0)
      curSeq->pop_back();   // Pop.
  }

  // Update current ctx.
  size_t curSize = curSeq->size();
  if (ctxPool.count(curSize) == 0)
    ctxPool[curSize] = new DenseSet<vector<int> * >;
  if (curSize == 0) {
    tidToCurCtxMap[tid] = *(ctxPool[0]->begin());
    return;
  }

  if (NORMAL_SLICING) {
    // Check whether current ctx is in the ctxPool.
    vector<int> ctx;
    for (size_t i = 0; i < curSize; i++)
      ctx.push_back(curSeq->at(i)->getOrigInstrId());
    CallCtx *found = findCtxInPool(ctx);

    // If not found, create new ctx and add to pool.
    if (!found) {
      vector<int> *newCtx = tidToCurCtxMap[tid] = new vector<int>;
      ctxPool[curSize]->insert(newCtx);
      for (size_t i = 0; i < curSize; i++)
        newCtx->push_back(ctx.at(i));
    } else
      tidToCurCtxMap[tid] = found;
  } else if (NORMAL_SLICING) {
    // TBD.
  } else if (RANGE_SLICING) {
    // TBD.
  } else {
    assert(false);
  }
}

void CallStackMgr::setCallStack(DynInstr *dynInstr) {
  uchar tid = dynInstr->getTid();
  if (tidToCallSeqMap.count(tid) == 0) {
    tidToCallSeqMap[tid] = new vector<DynCallInstr *>;
    tidToCurCtxMap[tid] = new vector<int>;
    ctxPool[0] = new DenseSet<std::vector<int> *>;
    ctxPool[0]->insert(tidToCurCtxMap[tid]);
  }
  
  // First, copy current per-tid callstack for current dynamic instruction.
  if (NORMAL_SLICING) {
    dynInstr->setCallingCtx(tidToCurCtxMap[tid]);
  } else if (NORMAL_SLICING) {
    // TBD.
  } else if (RANGE_SLICING) {
    // TBD.
  } else {
    assert(false);
  }
}

DynCallInstr *CallStackMgr::getCaller(DynInstr *dynInstr) {
  uchar tid = dynInstr->getTid();
  if (tidToCallSeqMap[tid]->size() == 0)
    return NULL; /* This is possible, e.g., instructions in main() and thread routines. */
  else
    return tidToCallSeqMap[tid]->back();
}

CallCtx *CallStackMgr::findCtxInPool(CallCtx &ctx) {
  size_t curSize = ctx.size();
  assert(curSize > 0);
  DenseSet<vector<int> * > *curPool = ctxPool[curSize];
  assert(curPool);
  DenseSet<vector<int> * >::iterator itr(curPool->begin());
  for (; itr != curPool->end(); ++itr) {
    vector<int> *curCtx = *itr;
    assert(curCtx->size() == ctx.size());
    for (int i = curSize-1; i >= 0; i--) { // From from tail to begin, easier to break, should be faster.
      if (curCtx->at(i) != ctx.at(i))
        break;
      else if (i == 0) {  // If all matched, return this ctx.
        return curCtx;
      }
    }
  }
  return NULL;
}

void CallStackMgr::verifyCtxPool() {
  llvm::DenseMap<int, llvm::DenseSet<std::vector<int> * > *>::iterator sizeItr(ctxPool.begin());
  // For each size.
  for (; sizeItr != ctxPool.end(); ++sizeItr) {
    size_t curSize = sizeItr->first;
    if (DBG)
      errs() << "CallStackMgr::verifyCtxPool is checking ctx size " << curSize << ".\n";
    DenseSet<vector<int> * > *curPool = sizeItr->second;
    assert(curPool);
    assert(curPool->size() > 0);
    DenseSet<vector<int> * >::iterator itr(curPool->begin());
    CallCtx *firstCtx = *itr;
    ++itr;
    for (; itr != curPool->end(); ++itr) {
      vector<int> *curCtx = *itr;
      if (DBG) {
        errs() << "CallStackMgr::verifyCtxPool is comparing ctx " << firstCtx << " and " << curCtx << ".\n";
        printCallStack(firstCtx);
        printCallStack(curCtx);
      }
      assert(curCtx->size() == curSize);
      for (int i = curSize-1; i >= 0; i--) { // From from tail to begin, easier to break, should be faster.
        if (curCtx->at(i) != firstCtx->at(i))
          break;
        else if (i == 0) {  // If there is the same ctx pointer with same ctx seq, assert false.
          assert(false);
        }
      }
    }
  }
}

void CallStackMgr::printCallStack(CallCtx *ctx) {
  errs() << "\nCtx size: " << ctx->size() << ": \n";
  for (size_t i = 0; i < ctx->size(); i++) {
    errs() << "Ctx[" << i << "]: ID: " << ctx->at(i)
      << ": " << stat->printInstr(idMgr->getOrigInstrCtx(ctx->at(i))) << "\n";
  }
}

void CallStackMgr::printCallStack(DynInstr *dynInstr) {
  vector<int> *ctx = dynInstr->getCallingCtx();
  if (NORMAL_SLICING)
    printCallStack(ctx);
}


