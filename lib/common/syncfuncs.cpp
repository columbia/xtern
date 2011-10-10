/* Author: Junfeng Yang (junfeng@cs.columbia.edu) */
#include <string.h>
#include "tern/syncfuncs.h"

#undef DEF
#undef DEFTERNAUTO
#undef DEFTERNUSER

namespace tern {
namespace syncfunc {

const int kind[] = {
  -1, // not_sync
# define DEF(func,kind,...) kind,
# define DEFTERNAUTO(func)      TernAuto,
# define DEFTERNUSER(func)  TernUser,
# include "tern/syncfuncs.def.h"
# undef DEF
# undef DEFTERNAUTO
# undef DEFTERNUSER
};

const char* name[] = {
  NULL, // not_sync
# define DEF(func,kind,...) #func,
# define DEFTERNAUTO(func)      #func,
# define DEFTERNUSER(func)  #func,
# include "tern/syncfuncs.def.h"
# undef DEF
# undef DEFTERNAUTO
# undef DEFTERNUSER
};

const char* nameInTern[] = {
  NULL, // not_sync
# define DEF(func,kind,...) "tern_"#func,
# define DEFTERNAUTO(func)  #func,
# define DEFTERNUSER(func)  #func"_real",
# include "tern/syncfuncs.def.h"
# undef DEF
# undef DEFTERNAUTO
# undef DEFTERNUSER
};

unsigned getNameID(const char* name) {
  for(unsigned i=first_sync; i<num_syncs; ++i)
    if(strcmp(name, getName(i)) == 0)
      return i;
  return not_sync;
}

unsigned getTernNameID(const char* name) {
  for(unsigned i=first_sync; i<num_syncs; ++i)
    if(strcmp(name, getTernName(i)) == 0)
      return i;
  return not_sync;
}

}
}
