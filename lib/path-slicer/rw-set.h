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
    bool intersectionStatic(DynInstr *brDynInstr, llvm::BasicBlock *notTaken,
      RWSet &staticSet);
    bool intersectionDyn(DynInstr *dynInstr, std::set<const llvm::Value *> &result);
    bdd getPointeeBDD();
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
    void addStatic(const llvm::Value *, DynInstr *dynInstr);
    void addDyn(DynOprd *dynOprd, const llvm::Value *maxSlicedV);
    bool isRead();
    bool isWrite();
    bool isStatic();
    bool isDyn();
    const char *getName();
    void print(const char *tag);
  };
}

#endif

