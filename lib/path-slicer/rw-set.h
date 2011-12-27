#ifndef __TERN_PATH_SLICER_RW_SET_H
#define __TERN_PATH_SLICER_RW_SET_H

#include <set>

#include "llvm/ADT/DenseSet.h"

#include "stat.h"
#include "dyn-instr.h"
#include "alias-mgr.h"
#include "cache-util.h"

namespace tern {
  class RWSet {
  private:
    Stat *stat;
    AliasMgr *aliasMgr;
    std::set<const llvm::Value *> staticSet;
    llvm::DenseMap<const llvm::Value *, std::set<DynOprd *> *> valueMap;
    CacheUtil dynInstrAliasCache;

    // BDD cache.
    bdd pointsTee;

    // Properties.
    bool readOrWrite;
    bool staticOrDyn;
    const char *name;
    
    

  protected:
    void addStatic(const llvm::Value *, DynInstr *dynInstr);
    void addDyn(DynOprd *dynOprd, const llvm::Value *maxSlicedV);
    void addBrToCache(DynInstr *dynInstr, llvm::BasicBlock *notTaken, bool result);
    void addToCache(std::vector<int> *ctx, llvm::Instruction *instr, bool result);
    bool inCache(std::vector<int> *ctx, llvm::Instruction *instr, bool &cachedResult);
    bool brInCache(int tid, std::vector<int> *ctx, llvm::Instruction *instr, 
      llvm::BasicBlock *notTaken, bool &cachedResult);

  public:
    RWSet();
    ~RWSet();
    void clear();
    void setProperty(bool rw, bool sd, const char *name);
    void printProperty();

    /* How to use (not implementation of this function):
    Given a dynamic branch instruction, first construct its "not taken branch" write set, then see 
    whether the write set intersects with current (read or write) set; then construct the 
    "not taken branch" read set, then see whether it intersects with current (must be write) set.
    The constructed write set uses the stored oprd + the callstack of current dynamic branch instr. */
    bool intersectionStatic(DynInstr *brDynInstr, llvm::BasicBlock *notTaken,
      RWSet &staticSet);

    /* How to use (not implementation of this function):
    Given a load instruction, see whether its loaded memory intersects with current (must be write)
    set; given a store instruction, see whether its stored memory intersects with current 
    (read or write) set. */
    bool intersectionDyn(DynInstr *dynInstr, std::set<const llvm::Value *> &result);

    /* This is the fast path of rwset. First we check the bdd and then the two intersection functions 
    agove. */
    bdd getPointeeBDD();

    /* Each rwset has its own property. Below are functions accessing them. */
    bool isRead();
    bool isWrite();
    bool isStatic();
    bool isDyn();
    const char *getName();
    void print(const char *tag);
  };
}

#endif

