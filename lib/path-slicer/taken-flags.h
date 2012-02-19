#ifndef __TERN_PATH_SLICER_TAKEN_FLAGS_H
#define __TERN_PATH_SLICER_TAKEN_FLAGS_H

#include <assert.h>

namespace tern {
namespace TakenFlags {

enum {
  NOT_TAKEN = 0,
# define DEF(target,kind) target,
# include "taken-flags.def.h"
# undef DEF
  num_taken_flags,
  first_taken_flag = 1
};

enum {TestTarget, EventTarget, RaceTarget, InterThreadTarget, CheckerTarget, IntraThreadTarget};

extern const char *takenReasons[num_taken_flags];

// Internal functions.
static inline const char* getReason(unsigned nr) {
  assert(first_taken_flag <= nr && nr < num_taken_flags);
  return takenReasons[nr];
}

// Public functions.
extern unsigned char getReasonID(const char* reason);

}
}

#endif

