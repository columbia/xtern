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
  return false;
}

void CacheUtil::add(void *high1, void *low1, void *high2, void *low2, bool result) {

}

void CacheUtil::del(void *high1, void *low1, void *high2, void *low2) {

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

