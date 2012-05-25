#include "cache-util.h"
using namespace tern;

using namespace std;
using namespace __gnu_cxx;

CacheUtil::CacheUtil() {

}

CacheUtil::~CacheUtil() {

}

void CacheUtil::clear() {
  mutualCache.clear();
  singleCache.clear();
}

bool CacheUtil::in(void *high1, void *low1, void *high2, void *low2, bool 
  &cachedResult) {
  PtrPair p1 = std::make_pair(high1, low1);
  PtrPair p2 = std::make_pair(high2, low2);
  PtrPairPair p = std::make_pair(p1, p2);
  if (DM_IN(p, mutualCache)) {
    cachedResult = mutualCache[p];
    return true;
  }
  return false;
}

void CacheUtil::add(void *high1, void *low1, void *high2, void *low2, bool result) {
  PtrPair p1 = std::make_pair(high1, low1);
  PtrPair p2 = std::make_pair(high2, low2);
  PtrPairPair p = std::make_pair(p1, p2);
  assert(!DM_IN(p, mutualCache));
  mutualCache[p] = result;
}

void CacheUtil::del(void *high1, void *low1, void *high2, void *low2) {
  PtrPair p1 = std::make_pair(high1, low1);
  PtrPair p2 = std::make_pair(high2, low2);
  PtrPairPair p = std::make_pair(p1, p2);
  assert(DM_IN(p, mutualCache));
  mutualCache.erase(p);
}

bool CacheUtil::in(void *high, void *low, bool &cachedResult) {
  PtrPair p = std::make_pair(high, low);
  if (DM_IN(p, singleCache)) {
    cachedResult = singleCache[p];
    return true;
  }
  return false;
}

void CacheUtil::add(void *high, void *low, bool result) {
  PtrPair p = std::make_pair(high, low);
  assert(!DM_IN(p, singleCache));
  singleCache[p] = result;
}

void CacheUtil::del(void *high, void *low) {
  PtrPair p = std::make_pair(high, low);
  assert(DM_IN(p, singleCache));
  singleCache.erase(p);
}

BddCacheUtil::BddCacheUtil() {

}

BddCacheUtil::~BddCacheUtil() {

}

bool BddCacheUtil::in(void *high, void *low) {
  PtrPair p = std::make_pair(high, low);
  if (DM_IN(p, bddCache))
    return true;
  else
    return false;
}

bdd BddCacheUtil::get(void *high, void *low) {
  PtrPair p = std::make_pair(high, low);
  assert(DM_IN(p, bddCache));
  return bddCache[p];
}

void BddCacheUtil::add(void *high, void *low, bdd pointee) {
  PtrPair p = std::make_pair(high, low);
  assert(!DM_IN(p, bddCache));
  bddCache[p] = pointee;
}


