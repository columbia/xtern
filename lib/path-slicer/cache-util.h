#ifndef __TERN_PATH_SLICER_CACHE_UTIL_H
#define __TERN_PATH_SLICER_CACHE_UTIL_H

#include "tern/type-defs.h"

#include "llvm/ADT/DenseSet.h"

namespace tern {
  class CacheUtil {
  private:
    /* Whether two dynamic instr may alias.
      Each dynamic instruction is <context pointer, instr pointer>, long long type, two of them form a pair. */
    llvm::DenseMap<LongLongPair, bool> mutualCache;

    /* Whether one dynamic instr has a property (true or false).
      Each dynamic instruction is <context pointer, instr pointer>, long long type. */
    llvm::DenseMap<long long, bool> singleCache;

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
    bool in(long high1, long low1, long high2, long low2, bool &cachedResult);

    /* Add two dynamic instructions with their may alias result to cache. */
    void add(long high1, long low1, long high2, long low2, bool result);

    /* Delete two dynamic instructions with their may alias result from cache. */
    void del(long high1, long low1, long high2, long low2);

    /* Whether one dynamic instruction has its property in cache.
      Each dynamic instruction is <context pointer, instr pointer>, long long type. 
      If yes, then the cachedResult is the returned property;
      if no, then the cachedResult makes no sense. */
    bool in(long high, long low, bool &cachedResult);

    /* Add one dynamic instruction with its property to cache. */
    void add(long high, long low, bool result);

    /* Delete one dynamic instruction with its property from cache. */
    void del(long high, long low);

  };
}

#endif

