#ifndef __TERN_PATH_SLICER_CACHE_UTIL_H
#define __TERN_PATH_SLICER_CACHE_UTIL_H

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
    void del(long high1, long low1, long high2, long low2);

    bool in(long high, long low, bool &cachedResult);
    void add(long high, long low, bool result);
    void del(long high, long low);

  };
}

#endif

