#include <string.h>
#include "taken-flags.h"

#undef DEF

namespace tern {
namespace TakenFlags {

const char* takenReasons[] = {
  "NOT_TAKEN", // not_taken
# define DEF(func,kind) #func,
# include "taken-flags.def.h"
# undef DEF

};

const int takenKind[] = {
  -1, // not_taken
# define DEF(func,kind) kind,
# include "taken-flags.def.h"
# undef DEF
};

const char* getReason(unsigned char reason) {
  assert(reason < num_taken_flags);
  return takenReasons[reason];
}

unsigned char getReasonID(const char* name) {
  assert(name && "got a null parameter.");
  for (unsigned char i=first_taken_flag; i<num_taken_flags; ++i)
    if (strcmp(name, getReason(i)) == 0)
      return i;
  return NOT_TAKEN;
}

int getReasonKind(unsigned char reason) {
  assert(reason < num_taken_flags);
  return takenKind[reason];
}

bool isTarget(unsigned char reason) {
  int kind = getReasonKind(reason);
  return (kind == TestTarget ||
    kind == EventTarget ||
    kind == RaceTarget ||
    kind == InterThreadTarget ||
    kind == CheckerTarget);
}

}
}

