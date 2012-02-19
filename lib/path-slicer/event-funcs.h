#ifndef __TERN_PATH_SLICER_EVENT_FUNCTIONS_H
#define __TERN_PATH_SLICER_EVENT_FUNCTIONS_H

#include <assert.h>

namespace tern {
namespace EventFuncs{
#undef DEF

enum {
  not_event = 0, // not a event function.
# define DEF(func,kind,...) func,
# include "event-funcs.def.h"
# undef DEF
  num_events,
  first_event = 1
};

enum {FileOp /* More types of events are to be added. */};

extern const char* eventName[];

// Internal functions of EventFuncs.
static inline const char* getName(unsigned nr) {
  assert(first_event <= nr && nr < num_events);
  return eventName[nr];
}

// Public functions.
extern unsigned getNameID(const char* name);
extern bool isEventFunc(const char *name);


}


}

#endif

