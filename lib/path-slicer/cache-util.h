#ifndef __TERN_PATH_SLICER_ALIAS_CACHE_H
#define __TERN_PATH_SLICER_ALIAS_CACHE_H

#include "tern/type-defs.h"

#include "llvm/ADT/DenseSet.h"

namespace tern {
  class CacheUtil {
  private:
    llvm::DenseMap<LongLongPair, bool> mutualCache;
    llvm::DenseMap<long long, bool> singleCache;

  protected:

  public:
    CacheUtil();
    ~CacheUtil();
    
    bool in(long high1, long low1, long high2, long low2, bool &cachedResult);
    void add(long high1, long low1, long high2, long low2, bool result);
    void delete(long high1, long low1, long high2, long low2);

    bool in(long high, long low, bool &cachedResult);
    void add(long high, long low, bool result);
    void delete(long high, long low);

  };
}

#endif

