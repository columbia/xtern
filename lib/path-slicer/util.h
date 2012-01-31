#ifndef __TERN_PATH_SLICER_COMMON_UTIL_H
#define __TERN_PATH_SLICER_COMMON_UTIL_H

#include "dyn-instrs.h"

namespace tern {
  class Util {
  private:

  protected:
    
  public:
    static llvm::Function *getFunction(llvm::Instruction *instr);
    static llvm::BasicBlock *getBasicBlock(llvm::Instruction *instr);
    static const llvm::Function *getFunction(const llvm::Instruction *instr);
    static const llvm::BasicBlock *getBasicBlock(const llvm::Instruction *instr);
    
    static bool isPHI(DynInstr *dynInstr);
    static bool isBr(DynInstr *dynInstr);
    static bool isRet(DynInstr *dynInstr);
    static bool isCall(DynInstr *dynInstr);
    static bool isLoad(DynInstr *dynInstr);
    static bool isStore(DynInstr *dynInstr);
    static bool isMem(DynInstr *dynInstr);
    
    static bool isPHI(const llvm::Instruction *instr);
    static bool isBr(const llvm::Instruction *instr);
    static bool isRet(const llvm::Instruction *instr);
    static bool isCall(const llvm::Instruction *instr);
    static bool isLoad(const llvm::Instruction *instr);
    static bool isStore(const llvm::Instruction *instr);
    static bool isMem(const llvm::Instruction *instr);

  };
}

#endif 

