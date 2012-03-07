#include <string.h>
#include "event-funcs.h"

#undef DEF

namespace tern {
namespace EventFuncs {

const char* eventName[] = {
  NULL, // not_event
# define DEF(func,kind,...) #func,
# include "event-funcs.def.h"
# undef DEF
};

const int eventType[] = {
  -1, // not_event
# define DEF(func,kind,...) kind,
# include "event-funcs.def.h"
# undef DEF
};

unsigned getNameID(const char* name) {
  //assert(name && "got a null parameter.");
  for(unsigned i=first_event; i<num_events; ++i)
    if(strcmp(name, getName(i)) == 0)
      return i;
  return not_event;
}

bool isEventFunc(const char *name) {
  return getNameID(name) != not_event;
}

}
}

