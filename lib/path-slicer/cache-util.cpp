#include "cache-util.h"
using namespace tern;

CacheUtil::CacheUtil() {

}

CacheUtil::~CacheUtil() {

}

void CacheUtil::clear() {
  mutualCache.clear();
  singleCache.clear();
}

bool CacheUtil::in(long high1, long low1, long high2, long low2, bool 
  &cachedResult) {

  return false;
}

void CacheUtil::add(long high1, long low1, long high2, long low2, bool result) {

}

void CacheUtil::del(long high1, long low1, long high2, long low2) {

}

bool CacheUtil::in(long high, long low, bool &cachedResult) {
  return false;
}

void CacheUtil::add(long high, long low, bool result) {

}

void CacheUtil::del(long high, long low) {

}

