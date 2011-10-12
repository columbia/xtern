/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef TERN_COMMON_SYNCFUNCS_H
#define TERN_COMMON_SYNCFUNCS_H

#include <cassert>
#include <limits.h>
#include <boost/static_assert.hpp>

namespace tern {
namespace syncfunc {

#undef DEF
#undef DEFTERNAUTO
#undef DEFTERNUSER

enum {
  not_sync = 0, // not a sync operation
# define DEF(func,kind,...) func,
# define DEFTERNAUTO(func)      func,
# define DEFTERNUSER(func)  func,
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERNAUTO
# undef DEFTERNUSER
  num_syncs,
  first_sync = 1
};
BOOST_STATIC_ASSERT(num_syncs<USHRT_MAX);

extern const int kind[];
extern const char* name[];
extern const char* nameInTern[];

enum {Synchronization, BlockingSyscall, TernUser, TernAuto};

static inline bool isSync(unsigned nr) {
  assert(first_sync <= nr < num_syncs);
  return kind[nr] == Synchronization;
}

static inline bool isBlockingSyscall(unsigned nr) {
  assert(first_sync <= nr < num_syncs);
  return kind[nr] == BlockingSyscall;
}

static inline bool isTern(unsigned nr) {
  assert(first_sync <= nr < num_syncs);
  return kind[nr] == TernUser || kind[nr] == TernAuto;
}

static inline bool isTernUser(unsigned nr) {
  assert(first_sync <= nr < num_syncs);
  return kind[nr] == TernUser;
}

static inline bool isTernAuto(unsigned nr) {
  assert(first_sync <= nr < num_syncs);
  return kind[nr] == TernAuto;
}

static inline const char* getName(unsigned nr) {
  assert(first_sync <= nr < num_syncs);
  return name[nr];
}

static inline const char* getTernName(unsigned nr) {
  assert(first_sync <= nr && nr < num_syncs);
  return nameInTern[nr];
}

unsigned getNameID(const char* name);
unsigned getTernNameID(const char* nameInTern);

} // namespace syncfuncs

} // namespace tern

#endif


