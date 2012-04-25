#ifndef __TERN_PATH_SLICER_CACHE_UTIL_H
#define __TERN_PATH_SLICER_CACHE_UTIL_H

#include "llvm/ADT/DenseSet.h"
#include <ext/hash_map>

#include "macros.h"
#include "type-defs.h"
#include <bdd.h>

namespace tern {
  class CacheUtil {
  private:
    /* Whether two dynamic instr may alias.
      Each dynamic instruction is <context pointer, instr pointer>, long long type, two of them form a pair. */
    HMAP<PtrPairPair, bool> mutualCache;

    /* Whether one dynamic instr has a property (true or false).
      Each dynamic instruction is <context pointer, instr pointer>, long long type. */
    llvm::DenseMap<std::pair<void *, void *>, bool> singleCache;

  protected:

  public:
    CacheUtil();
    ~CacheUtil();
    
    /* Clear all the two caches. */
    void clear();

    /* Whether two dynamic instructions have their may alias result in cache.
      Each dynamic instruction is <context pointer, instr pointer>, long long type. 
      If yes, then the cachedResult is the returned alias result;
      if no, then the cachedResult makes no sense. */
    bool in(void *high1, void *low1, void *high2, void *low2, bool &cachedResult);

    /* Add two dynamic instructions with their may alias result to cache. */
    void add(void *high1, void *low1, void *high2, void *low2, bool result);

    /* Delete two dynamic instructions with their may alias result from cache. */
    void del(void *high1, void *low1, void *high2, void *low2);

    /* Whether one dynamic instruction has its property in cache.
      Each dynamic instruction is <context pointer, instr pointer>, long long type. 
      If yes, then the cachedResult is the returned property;
      if no, then the cachedResult makes no sense. */
    bool in(void *high, void *low, bool &cachedResult);

    /* Add one dynamic instruction with its property to cache. */
    void add(void *high, void *low, bool result);

    /* Delete one dynamic instruction with its property from cache. */
    void del(void *high, void *low);
  };

  class BddCacheUtil {
  private:
    llvm::DenseMap<std::pair<void *, void *>, bdd> bddCache; 

  protected:

  public:
     BddCacheUtil();
     ~BddCacheUtil();

    /* Cache for bdd. */
    bool in(void *high, void *low);
    bdd get(void *high, void *low);
    void add(void *high, void *low, bdd pointee);
   };
}

#endif

