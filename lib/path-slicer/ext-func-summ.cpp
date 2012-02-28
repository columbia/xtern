#include <stdio.h>
#include <string.h>
#include "ext-func-summ.h"

#include <string>
using namespace std;

#undef DEF

namespace tern {
namespace ExtFuncSumm {

const char *extFuncName[] = {
  "other", // no-summary external functions.
# define DEF(func,...) #func,
# include "ext-func-summ.def.h"
# undef DEF
};

int argSumm[][ARG_LEN] = {
  {}, // no-summary external functions, this means all "0".
# define DEF(func,args...) {args},
# include "ext-func-summ.def.h"
# undef DEF  
};

extern void printAllSumm() {
  fprintf(stderr, "\n\nExtFuncSumm::printAllSumm:\n");
  for (int i = 0; i < num_ext_funcs; i++) {
    fprintf(stderr, "ExtFuncSumm::%s: {", extFuncName[i]);
    for (int j = 0; j < ARG_LEN; j++) {
      if (argSumm[i][j] == (int)ExtNone)
        fprintf(stderr, "ExtNone, ");
      else if (argSumm[i][j] == (int)ExtReadVal)
        fprintf(stderr, "ExtReadVal, ");
      else if (argSumm[i][j] == (int)ExtLoad)
        fprintf(stderr, "ExtLoad, ");
      else if (argSumm[i][j] == (int)ExtStore)
        fprintf(stderr, "ExtStore, ");
      else
        assert(false);
    }
    fprintf(stderr, "}\n");
  }
  fprintf(stderr, "\n\n");
}

unsigned getNameID(const char* name) {
  assert(name && "got a null parameter.");
  for (unsigned i=first_ext_func; i<num_ext_funcs; ++i) {
    std::string str1 = name;
    std::string str2 = getName(i);
    if (str1 == str2)
      return i;
  }
  return no_ext_summ;
}

bool extFuncHasLoadSumm(const char *name) {
  unsigned id = getNameID(name);
  for (int i = 0; i < ARG_LEN; i++) {
    if (argSumm[id][i] == ExtLoad)
      return true;
  }
  return false;
}

bool extFuncHasStoreSumm(const char *name) {
  unsigned id = getNameID(name);
  for (int i = 0; i < ARG_LEN; i++) {
    if (argSumm[id][i] == ExtStore)
      return true;
  }
  return false;
}
/*
bool extFuncHasSumm(const char *name) {
  return (extFuncHasLoadSumm(name) || extFuncHasStoreSumm(name));
}
*/
ExtSummType getExtFuncSummType(const char *name, unsigned argOffset) {
  unsigned id = getNameID(name);
  if (id == no_ext_summ)
    return ExtNone;
  assert(argOffset < ARG_LEN &&
    "Arg offset of an external function must be smaller than ARG_LEN.");
  return (ExtSummType)argSumm[id][argOffset];
}


}
}

