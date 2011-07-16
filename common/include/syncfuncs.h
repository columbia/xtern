/* Author: Junfeng Yang (junfeng@cs.columbia.edu) -*- Mode: C++ -*- */
#ifndef TERN_COMMON_SYNCFUNCS_H
#define TERN_COMMON_SYNCFUNCS_H

#include <cassert>
#include <limits.h>
#include <boost/static_assert.hpp>

namespace tern {
namespace syncfunc {

#undef DEF
#undef DEFTERN

enum {
# define DEF(func,kind,...) func,
# define DEFTERN(func,kind)     func,
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERN
  num_syncs,
  not_sync = (unsigned)(-1)
};
BOOST_STATIC_ASSERT(num_syncs<USHRT_MAX);

extern const int kind[];
extern const char* name[];
extern const char* nameInTern[];

enum {Synchronization, BlockingSyscall, TernBuiltin};

static inline bool isSync(unsigned nr) {
  assert(nr < num_syncs);
  return kind[nr] == Synchronization;
}

static inline bool isBlockingSyscall(unsigned nr) {
  assert(nr < num_syncs);
  return kind[nr] == BlockingSyscall;
}

static inline bool isTern(unsigned nr) {
  assert(nr < num_syncs);
  return kind[nr] == TernBuiltin;
}

static inline const char* getName(unsigned nr) {
  assert(nr < num_syncs);
  return name[nr];
}

static inline const char* getTernName(unsigned nr) {
  assert(nr < num_syncs);
  return nameInTern[nr];
}

unsigned getNameID(const char* name);
unsigned getTernNameID(const char* nameInTern);

} // namespace syncfuncs

} // namespace tern

#endif


