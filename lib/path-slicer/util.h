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
    /*
    static bool isPHI(DynInstr *dynInstr);
    static bool isBr(DynInstr *dynInstr);
    static bool isRet(DynInstr *dynInstr);
    static bool isCall(DynInstr *dynInstr);
    static bool isLoad(DynInstr *dynInstr);
    static bool isStore(DynInstr *dynInstr);
    static bool isMem(DynInstr *dynInstr);*/

    static bool isPHI(const llvm::Instruction *instr);
    static bool isBr(const llvm::Instruction *instr);
    static bool isUniCondBr(const llvm::Instruction *instr);
    static bool isRet(const llvm::Instruction *instr);
    static bool isCall(const llvm::Instruction *instr);
    static bool isIntrinsicCall(const llvm::Instruction *instr);
    static bool isLoad(const llvm::Instruction *instr);
    static bool isStore(const llvm::Instruction *instr);
    static bool isMem(const llvm::Instruction *instr);
    static bool hasDestOprd(const llvm::Instruction *instr);
    static llvm::Value *getDestOprd(llvm::Instruction *instr);

    /* Identify whether a Value* is constant or not. Filter out basicblocks (basic blocks can be operands
    of branch instructions, but basic blocks are not constant in LLVM, weird...). */
    static bool isConstant(const llvm::Value *v);
    static bool isConstant(DynOprd *dynOprd);

    /* The only place to determine whether a dynamic call instruction is creating a child thread. */
    static bool isThreadCreate(DynCallInstr *call);
    
    static void addTargetDataToPM(llvm::Module *module, llvm::PassManager *pm);
  };
}

#endif 

