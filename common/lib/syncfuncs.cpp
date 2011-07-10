/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <string.h>
#include "syncfuncs.h"

#undef DEF
#undef DEFTERN

namespace tern {
namespace syncfunc {

const int kind[] = {
# define DEF(func,kind,...) kind,
# define DEFTERN(func,kind)     kind,
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERN
};

const char* name[] = {
# define DEF(func,kind,...) #func,
# define DEFTERN(func,kind)     #func,
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERN
};

const char* nameInTern[] = {
# define DEF(func,kind,...) "tern_"#func,
# define DEFTERN(func,kind) #func,
# include "syncfuncs.def.h"
# undef DEF
# undef DEFTERN
};

unsigned getNameID(const char* name) {
  for(unsigned i=0; i<num_syncs; ++i)
    if(strcmp(name, getName(i)) == 0)
      return i;
  return not_sync;
}

unsigned getTernNameID(const char* name) {
  for(unsigned i=0; i<num_syncs; ++i)
    if(strcmp(name, getTernName(i)) == 0)
      return i;
  return not_sync;
}

}
}
