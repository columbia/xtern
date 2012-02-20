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

enum {TestTarget, EventTarget, RaceTarget, InterThreadTarget, CheckerTarget, IntraThread};

extern const char *takenReasons[];
extern const int takenKind[];

// Internal functions.


// Public functions.
extern const char* getReason(unsigned char reason);
extern int getReasonKind(unsigned char reason);
extern int getReasonID(unsigned char reason);
extern bool isTarget(unsigned char reason);

}
}

#endif

