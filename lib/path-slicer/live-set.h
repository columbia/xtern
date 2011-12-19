#ifndef __TERN_PATH_SLICER_LIVE_SET_H
#define __TERN_PATH_SLICER_LIVE_SET_H

#include <set>
#include <vector>

#include "llvm/ADT/DenseSet.h"

#include "dyn-instr.h"
#include "stat.h"

namespace tern {
  struct LiveSet {
  private:
    Stat *stat;

    /* The set of all dynamic operands in the live set. */
    std::set<struct DynOprd *> operands;

    /* The static version of all the dynamic oprds, used for fast lookup. */
    std::set<const llvm::Value *> operandsSet;

    /* Map from static value pointer to all its dynamic oprds, used for fast lookup. */
    llvm::DenseMap<const llvm::Value *, std::set<struct DynOprd *> *> vToOprdInfoMap;

    /*
    TBD: HOW TO SETUP THE CACHE FOR LIVE SET? SINCE IT IS VARIANT DURING SLICING.
    */

  protected:
    void printStaticSet(const char *tag);
    void addOprd(DynOprd *dynOprd);
    void delOprd(DynOprd *dynOprd);
    std::set<DynOprd *> &getDynOprdSet();
    std::set<const Value *> &getStaticSet();

  public:
    LiveSet();
    ~LiveSet();
    bool updateWritten(DynInstr *dynInstr);
    void addRead(DynInstr *dynInstr);
    size_t size();
    void clear();

    /* Check whether given the calling context of dynInstr, and given the static set of value pointers,
      whether it may modify the live set. The 'withInSameFunc' is important, since we need to check
      'def-use' or 'over-write' relations for static values, as given in the intra-thread algorithm. */
    bool vModifyLiveSet(DynInstr *dynInstr, std::set<const llvm::Value *> &valueSet,
      bool withInSameFunc = false);

    /* Check whether a static value with its context is in the live set. */
    bool vInLiveSet(const std::vector<int> &ctx, const llvm::Value *v);
  };
}

#endif

