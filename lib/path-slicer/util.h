#ifndef __TERN_PATH_SLICER_COMMON_UTIL_H
#define __TERN_PATH_SLICER_COMMON_UTIL_H

#include "dyn-instrs.h"

namespace tern {
  class Util {
  private:

  protected:
    
  public:
    static bool isPHI(DynInstr *dynInstr);
    static bool isBr(DynInstr *dynInstr);
    static bool isRet(DynInstr *dynInstr);
    static bool isCall(DynInstr *dynInstr);
    static bool isLoad(DynInstr *dynInstr);
    static bool isStore(DynInstr *dynInstr);
    static bool isMem(DynInstr *dynInstr);
  };
}

#endif 

