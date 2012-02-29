#ifndef __TERN_PATH_SLICER_EXTERNAL_FUNCTION_SUMMARY_H
#define __TERN_PATH_SLICER_EXTERNAL_FUNCTION_SUMMARY_H

#include <assert.h>

namespace tern {
namespace ExtFuncSumm {
#undef DEF

/* Currently all external functions have at most 3 arguments. This number has to be updated
if an external function with more than 3 arguments are added. */
#define ARG_LEN 3

enum {
  no_ext_summ = 0, // a function not having any external summary.
# define DEF(func,...) func,
# include "ext-func-summ.def.h"
# undef DEF
  num_ext_funcs,
  first_ext_func = 1
};

enum ExtSummType {ExtNone = 0, ExtReadVal, ExtLoad, ExtStore};

extern const char *extFuncName[];
extern int argSumm[][ARG_LEN];

// Internal functions.
static inline const char* getName(unsigned nr) {
  assert(nr < num_ext_funcs);
  return extFuncName[nr];
}

// Public functions.
extern void printAllSumm();
extern unsigned getNameID(const char* name);
extern bool extFuncHasLoadSumm(const char *name);
extern bool extFuncHasStoreSumm(const char *name);
extern ExtSummType getExtFuncSummType(const char *name, unsigned argOffset);
  
}


}

#endif


