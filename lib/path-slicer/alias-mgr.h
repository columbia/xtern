#ifndef __TERN_PATH_SLICER_ALIAS_MGR_H
#define __TERN_PATH_SLICER_ALIAS_MGR_H

#include <set>

#include "llvm/Instruction.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "aaol/aaol_client.h"
#include "bc2bdd/ext/JavaBDD/buddy/src/bdd.h"
#include "slicer/adv-alias.h"
#include "slicer/solve.h"

#include "dyn-instrs.h"
#include "cache-util.h"
#include "instr-id-mgr.h"
#include "stat.h"

namespace tern {
  class InstrIdMgr;
  struct AliasMgr {
  private:
    AAOLClient *origAaol;
    AAOLClient *mxAaol;
    AAOLClient *simAaol;
    slicer::AdvancedAlias *advAlias;
    InstrIdMgr *idMgr;
    CacheUtil aliasCache; /* Do we need multiple caches for each slicing mode? orig, mx, simple?
                                          Yes. TBD. */
    Stat *stat;
    std::set<const Type *> raceFreeTypes;
    

  protected:
    void appendCtxAliasCache(const std::vector<int> *ctx1, const llvm::Value *v1,
      const std::vector<int> *ctx2, const llvm::Value *v2, bool result);    
    void appendCtxAliasCache(const std::vector<int> *ctx1, long iid1, int opIdx1,
      const std::vector<int> *ctx2, long iid2, int opIdx2, bool result);
    bool inCtxAliasCache(const std::vector<int> *ctx1, const llvm::Value *v1,
      const std::vector<int> *ctx2, const llvm::Value *v2, bool &result);
    bool inCtxAliasCache(const std::vector<int> *ctx1, long iid1, int opIdx1,
      const std::vector<int> *ctx2, long iid2, int opIdx2, bool &result);

  public:
    AliasMgr();
    ~AliasMgr();

    /* This is the universal interface to query alias in all other modules. */
    //bool mayAlias(DynInstr *dynInstr1, int opIdx1, DynInstr *dynInstr2, int opIdx2);
    bool mayAlias(DynOprd *dynOprd1, DynOprd *dynOprd2);

    /* Get pointee bdd. In max slicing or range analysis mode, it should return the pointee of 
        the max sliced module. */
    bdd getPointTee(const std::vector<int> &ctx, const llvm::Value *v);
    bdd getPointTee(const std::vector<int> &ctx, int iid, int opIdx);
  };
}

#endif



// TBD: MUST CONSIDER THE CALLING CONTEXT OF SIMPLIFIED MODULE NOW.

