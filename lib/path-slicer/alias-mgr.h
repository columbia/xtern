#ifndef __TERN_PATH_SLICER_ALIAS_MGR_H
#define __TERN_PATH_SLICER_ALIAS_MGR_H

#include <set>

#include "llvm/Instruction.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "aaol/aaol_client.h"
#include "bc2bdd/ext/JavaBDD/buddy/src/bdd.h"
//#include "slicer/adv-alias.h"
//#include "slicer/solve.h"

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
    //slicer::AdvancedAlias *advAlias;
    InstrIdMgr *idMgr;
    CacheUtil aliasCache; /* Do we need multiple caches for each slicing mode? orig, mx, simple?
                                          Yes. TBD. */

    BddCacheUtil pointeeCache;
    long numPointeeQry;
    long numHitPointeeQry;

    Stat *stat;
    std::set<const Type *> raceFreeTypes;
    

  protected:
    void genInstrMap(llvm::Module &module,
      std::map<llvm::Instruction *, int> &insMap);
    AAOLClient *initAAOL(llvm::Module *module);
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
    void initStat(Stat *stat);
    void initInstrIdMgr(InstrIdMgr *idMgr);
    void initModules(llvm::Module *origModule, llvm::Module *mxModule,
      llvm::Module *simModule);

    /* This is the universal interface to query alias in all other modules. */
    //bool mayAlias(DynInstr *dynInstr1, int opIdx1, DynInstr *dynInstr2, int opIdx2);
    bool mayAlias(DynOprd *dynOprd1, DynOprd *dynOprd2);
    /* Context-insensitve version. */
    bool mayAlias(llvm::Value *v1, llvm::Value *v2);

    /* Get pointee bdd. In max slicing or range analysis mode, it should return the pointee of 
        the max sliced module. */
    bdd getPointTee(DynOprd *dynOprd);

    /* Given the calling context of a dynamic instruction and a static value pointer (not necessary
    from the given instruction), get the pointee of <ctx, v>. The v must have already been from
    either normal module or max sliced module, depending on normal or max slicing mode. */
    bdd getPointTee(DynInstr *ctxOfDynInstr, llvm::Value *v);
  };
}

#endif



// TBD: MUST CONSIDER THE CALLING CONTEXT OF SIMPLIFIED MODULE NOW.

