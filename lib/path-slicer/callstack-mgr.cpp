#include "util.h"
#include "callstack-mgr.h"
#include "path-slicer.h"
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

void CallStackMgr::init(Stat *stat, InstrIdMgr *idMgr, PathSlicer *pathSlicer) {
  this->stat = stat;
  this->idMgr = idMgr;
  this->pathSlicer = pathSlicer;
}

void CallStackMgr::updateCallStack(DynInstr *dynInstr) {
  uchar tid = dynInstr->getTid();
  std::vector<DynCallInstr *> *curSeq = tidToCallSeqMap[tid];
  Instruction *instr = idMgr->getOrigInstr(dynInstr);

  // Update call seq, push or pop.
  if (Util::isCall(instr) && pathSlicer->isInternalCall((DynCallInstr *)dynInstr)) {
    curSeq->push_back((DynCallInstr *)dynInstr);  // Push.
  } else {
    if (curSeq->size() > 0)
      curSeq->pop_back();   // Pop.
  }

  // Update current ctx.
  size_t curSize = curSeq->size();
  if (curSize == 0) {
    tidToCurCtxMap[tid] = *(ctxPool[0]->begin());
    return;
  }
  if (ctxPool.count(curSize) == 0)
    ctxPool[curSize] = new DenseSet<vector<int> * >;
  DenseSet<vector<int> * > *curPool = ctxPool[curSize];

  if (NORMAL_SLICING) {
    vector<int> ctx;
    for (size_t i = 0; i < curSize; i++)
      ctx.push_back(curSeq->at(i)->getOrigInstrId());

    DenseSet<vector<int> * >::iterator itr(curPool->begin());
    bool found = false;
    for (; itr != curPool->begin(); ++itr) {
      vector<int> *curCtx = *itr;
      for (int i = curSize-1; i >= 0; i--) { // From from tail to begin, easier to break, should be faster.
        if (curCtx->at(i) != ctx.at(i))
          break;
        else if (i == 0) {  // If all matched, mark found as true.
          found = true;
          tidToCurCtxMap[tid] = curCtx;
        }
      }
      if (found)  // If found, break and return.
        break;
    }

    // If not found, create new ctx and add to pool.
    if (!found) {
      vector<int> *newCtx = tidToCurCtxMap[tid] = new vector<int>;
      ctxPool[curSize]->insert(newCtx);
      for (size_t i = 0; i < curSize; i++)
        newCtx->push_back(ctx.at(i));
    }
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

DynCallInstr *CallStackMgr::getCallOfRet(DynRetInstr *dynRetInstr) {
  uchar tid = dynRetInstr->getTid();
  if (tidToCallSeqMap[tid]->size() == 0)
    return NULL; /* This is possible, e.g., instructions in main() and thread routines. */
  else
    return tidToCallSeqMap[tid]->back();
}

void CallStackMgr::printCallStack(DynInstr *dynInstr) {
  vector<int> *ctx = dynInstr->getCallingCtx();
  if (NORMAL_SLICING) {
    for (size_t i = 0; i < ctx->size(); i++) {
      errs() << "Ctx[" << i << "]: ID: " << ctx->at(i)
        << ": " << *(idMgr->getOrigInstrCtx(ctx->at(i))) << "\n";
    }
  }
}


