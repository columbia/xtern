#ifndef __TERN_PATH_SLICER_STAT_H
#define __TERN_PATH_SLICER_STAT_H

#include <string>
#include <ext/hash_map>

#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

#include "macros.h"
#include "stat.h"

namespace tern {
  class Stat {
  private:
    HMAP<long, llvm::raw_string_ostream * > buf;

  protected:

  public:
    struct timeval interSlicingSt;
    struct timeval interSlicingEnd;
    double interSlicingTime;

    struct timeval intraSlicingSt;
    struct timeval intraSlicingEnd;
    double intraSlicingTime;

    Stat() {
      interSlicingTime = 0;
      intraSlicingTime = 0;
    }

    ~Stat() {}
    
    void printStat(const char *tag = NULL) {
      // TBD.
    };

    const char *printInstr(const llvm::Instruction *instr, const char *tag) {
      long addr = (long)instr;
      if (HM_IN(addr, buf)) {
        return buf[addr]->str().c_str();
      } else {
        std::string *str = new std::string;
        llvm::raw_string_ostream *newStream = new llvm::raw_string_ostream(*str);
        instr->print(*newStream);
        buf[addr] = newStream;
        newStream->flush();
        return str->c_str();
      }
        return NULL;
    };
    
    const char *printValue(const llvm::Value *v, const char *tag) {
      return printInstr((const llvm::Instruction *)v, tag);
    };
  };
}

#endif

